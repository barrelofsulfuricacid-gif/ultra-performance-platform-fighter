# M4 real-simulation browser playtest

This checkpoint runs the production `pf_sim_tick` M4 movement, solid stage
geometry, four standing ground attacks, ground/wall/ceiling tech and
missed-impact recovery, reaction-driven tech chasing, directional air dodge,
wavedash/waveland, ledge-cancelling, bounded ledge regrabs/planking,
jump-canceled standing grab with capture/mash escape, production dash attack
with ordinary/boost-grab routes, a production two-hit jab with shield cancel
and a weak-hit jab-reset route,
and a data-defined grab pummel plus four directional throws with a
low-percent chain-grab route,
an optional four-player Team Wobble Lab with alternating allied down-throw and
fresh-grab handoffs against a legally mashing victim,
one fixed Relay Rod with pickup, carry, drop, directional throw, hit, and reset
plus bat-drop, glide-toss, and jump-cancel-throw routes,
one fixed Pulse Bolt with grounded/aerial fire, hit, shield block, and
powershield reflection plus short-hop-laser and bounded projectile-camping
routes,
one data-defined Prism Burst reflector with grounded/aerial physical hit and
active-box projectile reflection plus the Shine-spike route,
one data-defined Arc Reservoir with charge, storage cancel, exact resume, and
scaled grounded release,
one data-defined once-per-airtime Vector Ascent with horizontal steering,
ordinary gravity, spent-resource fall special, and landing/ledge restoration,
an authored two-tick shallow-back Moonwalk setup with facing-preserving reverse
slide and immediate/one-tick dashback controls,
an authored support-edge Teeter with neutral duration, standing-attack and
reverse-dash cancels, plus held-outward and early-release controls,
an authored one-tick crouch step with release-gated repetition and held,
neutral-down, and horizontal-only controls,
grounded jump-cancel attack with threshold and late-input controls,
light and strong production aerial routes with auto-cancel/L-cancel landing,
grounded forward/backward rolls, spot dodge, shield platform drop,
hit-reaction including grounded low-percent crouch cancel, and dense-shield
primitives plus a four-stock KO, moving revival platform, post-drop
invulnerability, sudden-death, and result/rematch loop and a deterministic
combat-event feed in
WebAssembly. It is no longer the
disposable M0 float32/Q16.16 comparison. The default duel's two visible
players, and all four fighters in the optional team lab, use the same validated
M4 fighter and stage content used by native, replay, rollback, and headless
execution.

## Owner evidence checklist

Expand `Owner evidence` below the live state/event cards. The build generates
its 61 ordered technique names and exact browser recipes directly from registry
schema 1, pinned source revision 2048934; a registry edit therefore cannot
silently leave the browser worksheet stale. Record each ordinary-input recipe
as observed pass, observed fail, or untested, and put reproduction detail on
every failure.

The same panel records the eight mandatory combat-rubric scores, critical
collision-anomaly confirmation, completed setup-to-results/rematch matches,
two-player input use, real Standard Gamepad use, environment/build identity,
and the explicit owner decision. Drafts persist in browser-local storage under
the evidence schema and source revision. `Export Markdown` produces the
repository-review record; `Export JSON` preserves the structured source data.
The page never changes registry status automatically.

Tactical and emergent rows remain observations of legal match sequences made
from independently verified constituent mechanics. Completing this worksheet
does not add, require, or imply a tactic-specific simulation harness.

## Replay-file inspector

The replay panel below the live playtest can download the generated canonical
format-1 replay and open a `.pfreplay` file. An opened file is copied into
WebAssembly, checked for container/chunk integrity and compatible identity,
then re-simulated through `pf_replay_verify_observed`. The displayed positions,
per-checkpoint SHA-256 hashes, and typed events therefore come from the verified
playback rather than a JavaScript decoder or stored presentation log.

The timeline shows the events entering the selected checkpoint and provides
Previous event / Next event navigation. A rejected checksum, schema, content,
config, tick hash, or result leaves the currently verified trace in place. This
M4 slice accepts files up to 1 MiB that match the canonical four-player
content/config fixture and its 500-tick bound; broader content discovery is not
silently inferred from replay metadata.

## Controls

| Action | Player 1 | Player 2 |
|---|---|---|
| Full left/right input and horizontal DI | `A` / `D` | Left / Right |
| Reduced-magnitude walk | `Shift+A` / `Shift+D` | `Shift+Left` / `Shift+Right` |
| Moonwalk | Dash, hold `Shift` plus the opposite direction for two ticks, then release `Shift` while keeping the direction | Same with Left / Right |
| Teeter cancel | Dash toward an edge, release the direction just before crossing, then Attack or press full opposite direction | Same with Left / Right |
| Crouch step / Stage humping | Tap `S` plus `A` or `D`, release, and repeat | Tap Down plus Left or Right, release, and repeat |
| Taunt / Taunt cancel | `T`; while dashing toward an edge, release horizontal and press `T` just before crossing | `,`; use the same edge timing |
| Scar Jump | From the right ledge, jump inward with `W`/Space, then freshly press full `D` at the raised-block wall; cancel with Jump or Attack | From the right ledge, jump inward with Up, then freshly press full Right at the wall; cancel with Jump or Attack |
| Ledge roll / ledge attack | After `LEDGE HANG` catch lock: fresh `G` rolls inward; fresh `F` or `H` attacks | After catch lock: fresh `.`/Numpad `1` rolls inward; fresh `/`/Numpad `0` or `'`/Numpad `2` attacks |
| Team Wobble Lab | Click `Team Wobble Lab`; physical Player 1 controls allied P1 | Physical Player 2 controls allied P3; P2 auto-mashes and P4 stays neutral |
| Jump | `W` or `Space` | Up |
| Up/down stick and vertical DI | `W` / `S` | Up / Down |
| Light attack; grounded reduced direction selects a tilt and full direction charges a smash; airborne full direction selects forward/back/up/down aerial | `F` | `/` or Numpad `0` |
| Immediate uncharged strong; grounded direction selects forward/up/down strong, airborne remains direct strong aerial | `H` | `'` or Numpad `2` |
| Special: neutral Pulse Bolt, full down Prism Burst, full up Arc Reservoir while grounded or Vector Ascent while airborne | `E` | `;` or Numpad `3` |
| Hold shield / tap tech, air-dodge, or L-cancel trigger | `G` | `.` or Numpad `1` |
| Standing, dash, jump-canceled, or boost grab | Hold `G`, tap `F`; for boost, tap `G` after starting dash attack with held `F` | Hold `.`/Numpad `1`, tap `/`/Numpad `0`; for boost, tap trigger after starting dash attack with held light |
| Pummel while holding a victim | Neutral/reduced direction + fresh `F` or `H` | Neutral/reduced direction + fresh `/`/Numpad `0` or `'`/Numpad `2` |
| Directional throw while holding a victim | Full direction + fresh `F` or `H` | Full direction + fresh `/`/Numpad `0` or `'`/Numpad `2` |
| Pick up / drop the nearby Relay Rod | Near the rod, or while holding it: hold `G`, tap `F` | Near the rod, or while holding it: hold `.`/Numpad `1`, tap `/`/Numpad `0` |
| Directional item throw | While holding the rod: direction + fresh `F` or `H` | While holding the rod: direction + fresh `/`/Numpad `0` or `'`/Numpad `2` |
| Jump-cancel attack | During `JUMP SQUAT`, hold full up and freshly press `F` or `H` | During `JUMP SQUAT`, hold full Up and freshly press `/`/Numpad `0` or `'`/Numpad `2` |
| Grounded forward/backward roll | Trigger + fresh `A` / `D` | Trigger + fresh Left / Right |
| Grounded spot dodge | Trigger + fresh `S` | Trigger + fresh Down |
| Shield platform drop | Hold trigger, then `Shift+S` | Hold trigger, then `Shift+Down` |
| Crouch, platform drop, fast fall | `S` | Down |
| Crouch cancel | Hold `S` until `CROUCH` before a low-percent hit | Hold Down until `CROUCH` before a low-percent hit |
| Reset/rematch both players | `R` or Reset/Rematch button | Same |
| Pause/resume | `P` or Pause button | Same |
| One tick while paused | `N` or Step button | Same |
| Toggle collision inspector | `I` or Collision Inspector button | Same |
| Return to local match setup | Match Setup / Change Setup button | Same |

The page opens in `Local 1v1 match setup` with the deterministic match paused
at tick zero. Choose 1–4 stocks, confirm the fixed Vector-versus-Vector Test
Stage pairing and input assignments, then press `Start Local Match`. Setup
input is cleared before the production duel is rebuilt with the selected
`pf_sim_config.stock_count`. Pause, step, reset, and the separate Team Wobble
lab stay disabled until the duel begins. A terminal stock result or time limit
pauses on the result banner; `Rematch` restarts the same configuration and
`Change Setup` returns to the stock selector. This is the temporary M4.3 local
loop, not the full M7 menu/navigation surface.

The collision inspector starts enabled. Green, cyan, pale-pink, and purple
stage lines identify the exact floor, moving one-way platform, stationary upper
one-way platform, and solid-block collision surfaces; the pink dashed rectangle
is the blast zone. Blue translucent rectangles are fighter
hurtboxes, gold rectangles are active attack hitboxes, and cyan rectangles are
active grabboxes. Relay Rod body/attack extents and the active Pulse Bolt
hitbox use the same collision-space transform. An invulnerable fighter keeps a
dashed gold hurtbox outline so geometry and hit rejection remain separately
visible. The legend and `data-collision-overlay-semantics` attribute expose
these meanings to the browser verifier. Toggling while paused redraws the
captured frame without consuming a simulation tick.

Up to two gamepads using the
[W3C Standard Gamepad layout](https://www.w3.org/TR/gamepad/#remapping) are
assigned in browser index order. On each pad, the left stick supplies analog
movement/DI, the D-pad supplies full
magnitude, the bottom face button is light attack, directional tilt, or charged
smash, the right face button is an immediate uncharged directional strong, the left face button
jumps, the top face button fires Pulse Bolt, down plus top face selects Prism
Burst, and up plus top face starts/resumes Arc Reservoir while grounded or
Vector Ascent while airborne. Back/View taunts, either bumper supplies a full
shield value, and the two analog triggers preserve their Standard Gamepad
button values as 16-bit shield strength for shield/tech/air-dodge/L-cancel
input. Light plus a bumper/trigger grabs. Keyboard and
gamepad inputs can be mixed for the same player. Non-standard browser mappings
are ignored rather than guessed.
In Team Wobble Lab, the two physical controller assignments deliberately map
to allied simulation slots P1 and P3. The default duel maps them to P1 and P2.

Browser view schema 47 contains the same 503 signed values as schema 46 while
making packed action-transition and coalesced-forfeit event meanings fail
closed. Each of the four player
blocks has a 53-value stride and appends shield-active, exact
left/right/top/bottom bounds, and signed x/y tilt after raw shield strength;
event count is at 236, event entries begin at 237, the item block begins at
397, the projectile block begins at 415, recovery availability begins at 427,
and four append-only revival-platform values per fixed player occupy 431–446.
Four 12-value stale-move records occupy 447–494: queue count, the selected
move's Q16.16 multiplier, the per-attack registration latch, and nine canonical
move IDs newest first. The thrown item's per-instance registration latch is at
495. The stationary upper platform's exact left, right, and y values are
append-only at 496–498. Four append-only prone-orientation values occupy
499–502. The state cards show raw shield strength, percentage, tilt, platform
activity, the readable stale queue, selected-move scale, registration, and
`prone none/back/stomach`.
Bubble fill and
stroke weight distinguish light from dense input, while the collision
inspector draws the authoritative shield AABB and the regular presentation
draws an ellipse inside those same bounds.

For the additional stage-geometry check, climb onto the raised block, jump
through the pale-pink deck from below, and fall back onto it. The fighter must
stop on the deck without horizontal carry. Hold down to fall through; while
shielding, use the reduced-down input to take the same shield-drop route. The
cyan center platform remains the visibly moving comparison and continues to
carry its grounded passenger.

Unmodified horizontal keys emit full stick magnitude and can enter initial
dash. Reversing them during the ten-tick initial-dash window performs a
dash-dance reversal. Holding `Shift` emits a reduced magnitude below the dash
threshold and therefore walks. `Shift` also reduces vertical magnitude, making
the shield-drop band distinct from full-down spot dodge.

The gold Relay Rod begins left of Player 1 and has its own live state card.
Walk near it and press light plus shield to pick it up. The same combination
drops it; do this airborne above the opponent for bat dropping, and repeat at
safe spacing for the miss control. While holding it, a fresh light or strong
attack plus direction performs one of four item throws. For glide toss, start a
grounded roll and press light during roll action frames 0–4; frame 5 retains the
roll and item. For jump-cancel throw, dash, press jump, then press light during
`JUMP SQUAT`; waiting until `AIRBORNE` keeps the jump and performs an ordinary
aerial item throw. Its green body outline remains visible while the inspector
is enabled; the gold attack overlay appears only while the item hitbox is
active.

The cyan Pulse Bolt has its own live state card. Press `E` or `;`/Numpad `3`
to fire from the ground, or tap jump, release during jump squat, and fire after
takeoff for short-hop laser. Only one bolt can occupy the canonical slot. An
ordinary shield blocks and clears it; activate shield during the authored
two-frame projectile window to reverse the bolt, transfer its owner, and take
no damage. The event feed distinguishes fire, hit, and reflection.

For projectile camping, keep the opponent across the stage and freshly press
Special whenever the previous canonical Pulse Bolt has resolved. The automated
recipe runs this legal one-slot loop for 180 simulation ticks while the
opponent continuously approaches and jabs: seven fire actions produce six
hits, keep at least 10.58 units of center separation, and leave the camper at
0%. Its Reset control omits the bolts and requires the same opponent policy to
close the distance and land three physical hits.

Hold down and freshly press the same special control for Prism Burst. Neutral
special continues to fire Pulse Bolt. Prism Burst has one startup tick, two
active ticks, and nine recovery ticks; its physical box deals 3% and launches
downward. If a Pulse Bolt overlaps the active box, it reverses horizontal
velocity and changes owner without setting the powershield indicator. For the
Shine-spike route, follow an offstage opponent and place `PRISM BURST AIR` into
their recovery path; the readiness oracle also proves the same victim recovers
when left unchallenged.

Hold full up and freshly press the same special control for Arc Reservoir.
The `ARC RESERVOIR CHARGE` state adds one meter tick per simulation tick up to
120. Press shield to enter `ARC RESERVOIR STORE`, then release shield before
the four-tick store animation finishes: the meter remains and the ordinary
grounded controls return immediately, including a same-tick light attack.
Enter again with up plus fresh special to resume at the exact stored value,
then press light to use `ARC RESERVOIR RELEASE`; its damage scales from 4% to
20%. Holding shield through all four store ticks enters ordinary shield, while
being hit during charge/store clears the meter.

While airborne, hold full up and freshly press the same special control to
enter `VECTOR ASCENT`. The fighter receives the authored upward launch, may
steer horizontally during the 18-tick ascent under ordinary gravity, then
enters `FALL SPECIAL`. The player card changes Vector Ascent from `READY` to
`SPENT`; another up-special is ignored until landing, ledge grab, respawn, or
Reset restores it.

For the emergent gimp route, knock the opponent offstage, let them begin an
inward-steered Vector Ascent, then intercept it with the light aerial or
`PRISM BURST AIR`. Confirm the spent recovery cannot be repeated and compare
with an unchallenged reset where the opponent reaches the ledge or stage. For
the stage-spike route, build damage and meet the recovering opponent beside
the raised block's outer wall. Send them into that wall with an aerial; omit
the trigger for an outward `WALL BOUNCE`, then reset and freshly press the
trigger before impact for the `WALL TECH` survival control. The underside of
the block provides the corresponding `CEILING BOUNCE` / `CEILING TECH`
comparison. These emergent routes reuse the independently checked recovery,
hit, surface-impact, and stock mechanics rather than separate technique-only
startup scripts.

For a fox-trot, tap and release one full direction, then repeat that same
direction. Every fresh tap returns the inspector to tick 1 of `INITIAL DASH`
and the neutral tick preserves a short traction slide. Holding the direction
instead reaches `RUN` after the data-defined ten-tick window; using the
`Shift`/reduced-magnitude input after release produces `WALK`, not another dash.

For a Moonwalk, begin a full forward dash, hold `Shift` plus the opposite
horizontal direction for exactly two simulation ticks, then release `Shift`
while keeping the opposite direction held. `MOONWALK SETUP` becomes
`MOONWALK`; the fighter keeps the original facing while sliding backward.
Release the direction to see ordinary traction preserve the diminishing slide.
Switching to full back immediately or after only one reduced-back tick instead
turns the fighter into an ordinary opposite `INITIAL DASH`.

For a Teeter cancel, dash toward the floor edge and release the horizontal key
just before residual momentum would carry the fighter off. The inspector names
the exact clamped edge state `TEETER`. Press Attack immediately for a standing
attack, or press the full opposite direction for a fresh reverse
`INITIAL DASH` without waiting through run brake. Hold outward continuously to
confirm the ordinary run-off, and release well before the edge to confirm the
fighter stops short. Leaving `TEETER` neutral shows its authored 30-tick
duration before returning to `IDLE`.

For Stage humping, tap diagonal down-forward or down-back from standing or
`CROUCH`. Each fresh diagonal-down edge enters `CROUCH STEP`, moves exactly
0.1 unit for one simulation tick, and settles into ordinary `CROUCH`. Release
the keys and repeat to chain the microsteps. Holding the diagonal produces
only the first step; neutral down remains a stationary crouch, and horizontal
alone remains an ordinary dash.

For a Taunt cancel, dash toward the floor edge, release the horizontal key,
and freshly press Taunt just before retained momentum crosses the support
bound. The inspector briefly routes the authored action through the canonical
edge check and ends that tick in `TEETER`, far earlier than the normal
90-tick taunt recovery. Reset and press Taunt near center stage to compare the
full duration; attack, jump, and direction inputs remain locked, and holding
Taunt cannot retrigger it without a release. On a standard gamepad, Back/View
is Taunt.

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

For a jump-cancel attack, begin a dash, press jump, then keep full up held and
freshly press light or strong during `JUMP SQUAT`. The grounded `UP STRONG`
starts while inherited dash momentum keeps sliding under traction. Neutral or
reduced-magnitude up continues jump squat; waiting until `AIRBORNE` performs the
ordinary aerial attack instead. Light plus shield remains jump-canceled grab,
and attacking with the Relay Rod held remains jump-cancel throw.

For a dashing shield, reach `RUN`, tap shield for one tick, and release it.
The inherited run momentum continues sliding under traction through the
eight-tick minimum hold and 15-tick `SHIELD RELEASE`. Repeat while holding
shield: the travel path is the same, but the fighter remains `SHIELD`. A shield
tap from idle is the no-travel negative case.

For a jump-canceled grab, begin `INITIAL DASH`, press jump, then hold shield
and tap light attack during `JUMP SQUAT`. The fighter enters the standing
`GRAB` while retaining forward slide, the exact two active frames draw a cyan
grabbox, and contact enters reciprocal `GRAB HOLD` / `GRABBED` states. The
victim card displays `MASH OUT · Nf`; alternate fresh full-left/right/down or
button edges to reduce it faster than waiting. The feed records both `GRABBED` and
escape events. Light-plus-shield directly during initial dash is rejected, as
is the same combination after takeoff. From idle, light-plus-shield remains
the ordinary standing-grab route, while the same combination from `RUN` is the
ordinary dash-grab route.

For a boost grab, first hold a full direction until `RUN`, press and keep
holding light attack to enter `DASH ATTACK`, then freshly press shield on the
next, second, or third stored action tick. Those inputs correspond to
dash-attack frames 2–4 and cancel into the same standing `GRAB` while retaining
the faster dash-attack slide. Compare light plus shield together directly from
`RUN`: that ordinary dash grab carries only run momentum and whiffs at the
focused route's extended spacing. Waiting one more tick before adding shield
leaves `DASH ATTACK` intact. If uncanceled, the default attack has four startup
ticks, three active ticks, deals 8%, and appears as `DASH ATTACK` in both the
state card and typed hit feed.

For a jab cancel, press light from standing to enter `GROUND ATTACK`. Freshly
press shield while the state card shows action tick 4 through 7; the first jab
enters `SHIELD` immediately on both hit and whiff. Hold shield starting on tick
3 and keep holding it into tick 4 to confirm an early edge is not buffered,
then freshly press it on tick 8 to confirm the first-late rejection. Instead
freshly press light during ticks 4–7 to select the independent `JAB FINAL`;
the state card and typed hit feed identify its 7% second hit separately. This
follows the documented
[jab-cancelling](https://www.ssbwiki.com/Jab_cancelling) shield route.

For a jab reset, strong-launch the opponent, omit the tech input, follow their
landing, and wait for the state card to show `DOWN WAIT`. Jab before they choose
a getup. The default 6% hit freezes normally, then produces the small 12-tick
`RESET BOUND` before 30 vulnerable ticks of `FORCED GETUP`; attack and jump stay
locked until it ends. Pause and step to choose a getup on the jab's collision
tick and confirm its existing gold invulnerability ring rejects the hit.
Alternatively, cross two stick-component thresholds during reset hitlag and
hold away for ASDI; enough displacement keeps the target airborne when the
bound expires, so the card returns to `AIRBORNE` and permits an aerial instead
of forcing a getup. This follows the documented weak-hit reset and SDI escape
behavior for a [jab reset/lock](https://www.ssbwiki.com/Spooky_stun).

While in `GRAB HOLD`, freshly press either attack with neutral or reduced
direction to enter `PUMMEL`. At tick 2 the victim takes 3%, the feed records a
typed non-launching pummel, and both grab links remain active; after ten total
ticks the holder returns to `GRAB HOLD`. Holding the attack through that return
does not repeat it without a fresh edge. Keep a full direction held with a
fresh attack to throw instead. Left/right selects forward or back relative to
the thrower's facing; up/down selects the vertical throw only when that axis is
strictly dominant, so horizontal wins a diagonal tie.
The victim remains tethered until the authored release tick, then both links
clear, both players enter hitlag, and the feed records the selected throw,
damage, and launch vector. Down throw at low percent leaves enough proximity to
pursue and regrab twice; at 96% with outward DI, the same earliest standing
regrab whiffs and emits no new grab event.

For a shield platform drop, land on the pass-through platform and raise shield.
On a later tick, keep holding the trigger and press reduced down with
`Shift+S` / `Shift+Down`, or tilt a gamepad stick into the same analog band.
The fighter immediately enters `AIRBORNE` with the ordinary nine-tick platform
pass timer. Too little down remains `SHIELD`, while full down enters
`SPOT DODGE`; releasing the trigger after minimum hold instead enters grounded
`SHIELD RELEASE`. This follows the documented Melee
[shield drop](https://www.ssbwiki.com/Shield_drop) interaction.

For a small-step forward smash, press full direction and light attack together
for the standing charge comparison, then release light. Reset, tap and hold the
same full direction, wait one to three simulation ticks, then press light. The
fighter enters `FORWARD STRONG CHARGE` after traveling forward; hold light to
charge and release it for `FORWARD STRONG`. Waiting four ticks instead produces
`FORWARD ATTACK`. The dedicated strong key/button remains the immediate
uncharged route.

From standing, reduced forward/up/down plus fresh light enters `FORWARD
ATTACK`, `UP ATTACK`, or `DOWN ATTACK`; neutral remains `GROUND ATTACK`. Full
forward/up/down instead enters the matching smash charge, with equal diagonals
using horizontal priority. Hold light for up to 60 simulation ticks, watch the
HUD counter, then release; damage scales linearly by up to +50% and tick 60 releases
automatically. The dedicated strong key plus direction enters the same
forward/up/down strong immediately with zero charge. Horizontal geometry and
launch still mirror with facing.

While airborne, neutral or reduced stick plus a fresh light press retains the
8% neutral `AERIAL ATTACK`. Full vertical-dominant input selects the 9% `UP
AERIAL` or 10% `DOWN AERIAL`; full horizontal-dominant or equal-diagonal input
selects the 10% `FORWARD AERIAL` or 11% `BACK AERIAL` relative to current
facing. All five light aerials retain ordinary drift, auto-cancel, 12-tick
landing lag, and six-tick L-cancel landing. The dedicated strong key remains
the direct `STRONG AERIAL ATTACK` route regardless of stick direction.

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

After a legal air jump, the action card shows `DELAYED AIR JUMP` for the
default six-tick window. Fresh light or strong attack on action ticks 0–5
cancels the remaining upward rise and enters the matching aerial immediately;
waiting until the action has returned to `AIRBORNE` preserves the full jump
arc. Pressing jump and attack together while ordinarily airborne selects the
attack without consuming the displayed air jump.

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

For a ledge-cancel, use the stationary fixture and full hop through its narrow
platform. During descent, hold down-right and press a fresh trigger so
`SPECIAL LANDING` begins beside the right edge. The retained slide crosses the
support bound on the first recovery tick and immediately produces ordinary
`AIRBORNE`. The startup probe repeats the same inputs at platform center and
requires the entire action-tick 0–9 landing lock before idle, then restores the
default live-playtest content.

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

For a crouch cancel, leave the defender grounded and hold down until the state
card says `CROUCH`, then have the opponent jab. Compare against the same jab
while standing: damage and four-tick hitlag stay equal, while launch and
hitstun become exactly 2/3 and the event feed appends `CROUCH CANCEL`. The
resulting damage must be at or below 40%; repeat with enough prior damage that
the jab ends above 40% and confirm ordinary launch/hitstun with no flag. Throws
and airborne contact also use the ordinary reaction path.

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
returns at zero damage on a glowing player-colored revival platform. It
descends for 30 frames while input is ignored and the fighter is
collision-invulnerable. Once it stops, movement, an attack/special/jump/shield
input, or the 90-frame neutral timeout drops the fighter. The 120-frame
hitbox-rejecting timer and dashed gold ring begin at that drop; ordinary
movement and attacks remain available. Losing the final stock enters
`ELIMINATED`, pauses the match, displays the winner, and changes Reset to
Rematch.

If both players lose their final stock on the same tick, neither is awarded an
immediate win. The page displays `SUDDEN DEATH · 300%`, gives each fighter one
stock, waits through the same respawn countdown, and returns both at 300%. A
second simultaneous KO resolves deterministically to Player 1 rather than
looping forever.

To grab a ledge, fall beside it while facing inward. After the seven-tick catch
window, press toward the stage to climb, freshly tap the trigger to roll,
freshly press either attack key for ledge attack, press down or away to release,
or press jump for a ledge jump. Roll shows a 30-tick inward transition;
ledge attack shows its active box after six startup ticks and deals 10%. A
claimed ledge rejects another fighter until its current occupant releases or
completes the climb, roll, or attack.

For a Scar Jump, catch the right ledge and wait for `LEDGE HANG`, then press
Jump and hold left/inward so the ledge jump travels into the raised block.
Freshly press full right/away when the fighter touches its wall. `WALL JUMP`
launches outward and upward without spending the air jump; press Jump to spend
that saved jump or Attack to cancel into an aerial. Holding right immediately
after the ledge jump is the negative route: it changes the approach before wall
contact and never enters `WALL JUMP`.

For a Team Wobble handoff, click `Team Wobble Lab`. P1 and P3 are allied and
stand on opposite sides of P2; P2 emits ordinary alternating mash edges only
while captured, and P4 stays neutral. Capture P2 with one ally, then hold down
and freshly press Attack with that holder while the opposite ally freshly
presses light plus shield. The low throw releases P2 into the waiting grab.
Repeat from the opposite side. For the negative route, start the waiting grab
during the initial capture and confirm its active window expires before release
so P2 escapes. Click `Return to Duel Lab` to restore the ordinary item match.

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

Every jump, down, or outward release from `LEDGE HANG` starts a separate
29-tick disabled-regrab period. An otherwise legal catch with one remaining
tick is rejected; the next tick may catch and refresh the 37-tick dashed ring.
The startup `planking_probe` uses a narrow data fixture to perform three exact
drop/double-jump/regrab refreshes while Player 2's jab is active, then
fast-falls for the final two ticks in the negative route so the same jab hits.
The fixture is restored before the live default match is rendered.
That same ordinary-input route is the Stalling recipe: repeat the safe cycle
to delay engagement, then use the fast-fall control to confirm the resource and
vulnerability limit. Stalling is emergent and therefore shares the existing
Planking native/browser evidence rather than adding a duplicate probe.

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
from the rendered state. It shows canonical sequence/tick labels for packed
per-player action transitions, hits and their tumble/crouch-cancel flags,
shield interactions, grabs, pummels, escapes, throws, KOs, respawns, revival
drops, sudden death, results, coalesced multi-player forfeits, and time limits.
Action rows decode the previous and final action name for every bit in the
changed-player mask. The simulation returns
at most 16 records for the current tick; the
browser keeps only the newest ten as non-authoritative presentation history
and clears them on Reset or any observed rewind.

During ordinary target hitlag, crossing into a new horizontal or vertical stick
component produces one SDI pulse. Holding that component does not repeat it.
The final hitlag input also supplies ASDI and trajectory DI, with full
perpendicular input reaching the configured 18-degree maximum.

`G`, `.`, and Numpad `1` drive one normalized full trigger. On a Standard
Gamepad, bumpers are likewise full, while analog trigger buttons 6 and 7 retain
their browser-reported pressure. Hold from idle, walk, crouch, or run to raise
a shield; initial dash cannot shield until it reaches run. Input below 8,192
does not shield, 8,192–32,767 is light, and 32,768 or higher is dense. A
run-to-shield transition keeps momentum and slides under traction as a shield
stop. The translucent player-color bubble shrinks with the inspected 60-point
shield health; its opacity/stroke and the state-card strength percentage/raw value
show the current analog strength.

At the light threshold, holding shield drains 0.07 health per tick; depletion
interpolates to 0.28 at the dense threshold. Releasing before eight ticks keeps
the shield active at its last strength until that minimum completes, then starts
the 15-tick `SHIELD RELEASE`; jumping cancels an already active shield or
release. Shield health regenerates by 0.07 per non-shield tick.

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
`SHIELD STUN`. Light shield keeps the same block damage/stun but receives more
defender pushback as pressure approaches the light threshold. Raising a dense
shield within four ticks of contact powershields the attack: the defender takes
no shield damage but keeps ordinary physical hitlag/stun and receives the larger
Melee-style pushback. Light shield cannot powershield. The state card shows
shield health, raw strength, shield stun, and a powershield indicator.

While ordinary shield is active, move the main stick outside the dead zone to
tilt its collision volume up to 0.3 world unit on each axis; return the stick to
neutral to recenter it. Light shield begins larger than dense shield at equal
health, and either volume shrinks with lost shield health down to its authored
minimum scale. Enable the collision inspector to compare the violet shield AABB
with the blue hurtbox. An attack touching the shield blocks even outside the
body; an attack touching only exposed blue hurtbox is an ordinary shield poke.
Tilt and raw strength freeze through shield hitlag/stun and remain visible in
the state card.

During grounded shield hitlag, crossing the horizontal stick threshold produces
one shield-SDI pulse at 0.66 of the ordinary SDI distance. Holding the same
horizontal component does not repeat it, and vertical additions or vertical-only
input are ignored. The final horizontal hitlag input produces one smaller
shield-ASDI shift at 0.66 of ordinary ASDI when `SHIELD STUN` begins; shield
reaction never applies trajectory DI.

After a physical powershield, release shield by the end of shield stun. Frame 1
of `SHIELD RELEASE` cannot start a ground attack; a fresh attack press on frame
2 cancels the remaining release animation into the selected neutral, up, down,
or strong standing attack. An early
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
ATTACK`. Neutral getup lasts 30 ticks with 23 invulnerable; every getup roll
lasts 35 ticks, but its movement start and inclusive invulnerability are
back/forward 6 and 1–19, back/backward 12 and 12–29, stomach/forward 8 and
1–19, or stomach/backward 5 and 1–24. Floor attack lasts 49 ticks with 26
invulnerable.
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

All current standing ground actions share the same powershield-cancel selector.
The SDI registry row remains `playable` pending the mandatory owner playtest and
broader acceptance evidence.

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
   down/away release, jump, inward climb, fresh-trigger `LEDGE ROLL`, and fresh
   light/strong `LEDGE ATTACK`. Hold trigger or attack during catch lock and
   confirm it does not repeat when the hang becomes actionable. For roll,
   confirm the fighter travels inward and lands after 30 ticks; for attack,
   confirm the active box, 10% hit, and on-stage landing.
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
    sequenced 6% `hit` entry naming both players. Reset and repeat with reduced
    forward/up/down plus light for 7% `FORWARD ATTACK`, 9% `UP ATTACK`, and 8%
    `DOWN ATTACK`. Use full forward/up/down plus light to enter each charge,
    release early, then hold one to the 60-tick automatic release and confirm
    the HUD counter and increased damage. Use the dedicated strong key with
    neutral/forward/up/down input to confirm immediate uncharged `STRONG
    ATTACK`, `FORWARD STRONG`, `UP STRONG`, and `DOWN STRONG`. Then jump and
    freshly press light with neutral, full forward, full
   back, full up, and full down input. Confirm `AERIAL ATTACK`, `FORWARD
   AERIAL`, `BACK AERIAL`, `UP AERIAL`, and `DOWN AERIAL` labels; compare
   8/10/11/9/10% damage and the visible signed launch directions. Repeat an
   equal full diagonal for horizontal priority and use the dedicated strong
   key with a direction to confirm it still enters `STRONG AERIAL ATTACK`.
17. Attack facing away and confirm the active hitbox whiffs. Reset, bring both
    players into range, and attack on the same tick to confirm a simultaneous
    trade.
18. Pause just before contact and use `N` to step through hitlag. On the first
    target hitlag tick, press a horizontal direction and confirm `SDI pulses`
    becomes 1 and the target shifts. Keep holding it for another tick and
    confirm the count stays 1. Add the vertical component and confirm the count
    becomes 2. Reset at zero percent, hold down until the target says `CROUCH`,
    and jab it; compare with a standing jab and confirm equal damage/hitlag,
    visibly reduced launch/hitstun, and `CROUCH CANCEL` in the typed feed. Build
    enough prior damage for the jab to finish above 40% and confirm the label
    and reduction disappear.
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
    directions, and either attack key for `FLOOR ATTACK`. Use the state card to
    record whether the landing is `prone back` or `prone stomach`, then repeat
    both roll directions for each orientation. Confirm the four movement and
    gold-ring boundaries in the route table above. Pause and step
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
26. From idle, hold the shield key. Confirm the full-density bubble appears on
    frame 1, health drains, an early key release waits for the eight-tick
    minimum, and `SHIELD RELEASE` lasts 15 ticks. With a Standard Gamepad,
    compare a trigger just below the 12.5% light threshold, exactly at/above it,
    midway toward the 50% dense threshold, and fully pressed; confirm the state
    card preserves raw strength, the lighter shields drain more slowly, and the
    light bubble is larger at equal health. With the inspector enabled, move
    the stick while shielding and confirm the violet AABB and card tilt move
    together while the blue hurtbox stays fixed; release the stick to recenter.
    Press jump during shield/release and
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
    defender resumes in `SHIELD STUN`. Pause before contact and step the freeze:
    cross the defender's horizontal threshold once and confirm `SDI pulses`
    becomes 1 with a horizontal shift; hold it, then add vertical input, and
    confirm the count stays 1 with no vertical shift. Keep horizontal held on
    the final hitlag tick and confirm a second, smaller horizontal ASDI shift as
    `SHIELD STUN` begins. Repeat by raising shield immediately before contact;
    confirm the powershield indicator appears, shield health loses only its
    normal hold depletion, and pushback is larger.
29. After that powershield, release shield before `SHIELD STUN` ends. Leave the
    first `SHIELD RELEASE` tick neutral, then press the defender's attack key
    on frame 2 and confirm it enters `GROUND ATTACK`. Repeat with reduced
    forward/up/down plus light and directional direct strong to confirm all
    immediate ground normals use the same cancel without entering charge.
    Repeat after an
    ordinary block and confirm none can skip the 15-tick release.
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
    `RESPAWN WAIT` counts down from 60, then the fighter returns at zero percent
    on the glowing revival platform. Press attack during the descent and
    confirm it stays locked and collision-invulnerable. After the platform
    stops, move to drop; confirm the platform disappears, the feed records
    `REVIVAL DROP · player input`, and the dashed ring starts at 120 frames.
    Move and attack during that timer, then confirm the ring expires. Repeat
    once while staying neutral through the hold and confirm `automatic
    timeout`. Repeat until the final stock and confirm the result banner,
    paused match, and Rematch button. Confirm the feed records KO, respawn,
    revival drop, and final match result in increasing sequence order.
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
45. From idle, press light plus shield to capture Player 2. While the card shows
    `GRAB HOLD`, keep the stick neutral and freshly press either attack. Confirm
    `PUMMEL`, one typed 3% event at tick 2, retained links, and the return to
    `GRAB HOLD` after ten ticks. Release attack, then hold full down and freshly
    press either attack. Confirm `DOWN THROW`, link clearing at release, shared
    hitlag, 6% damage, and a typed throw feed entry. Pursue and repeat until two
    regrabs complete. Then build
    Player 2 to high percent, hold outward DI through the same down throw, and
    confirm the earliest standing regrab whiffs rather than starting a new
    capture.
46. Put the fighters close, press Player 1 light, and freshly press shield on
    first-jab action tick 4; confirm the hit cancels into `SHIELD`. Repeat at
    whiff range on tick 7. Hold shield from tick 3 into tick 4, then freshly
    press it on tick 8, and confirm neither route cancels. Finally press light
    freshly during ticks 4–7 and confirm `JAB FINAL` deals the separately typed
    7% second hit.
47. Strong-launch Player 2, omit tech, follow the missed landing into `DOWN
    WAIT`, and jab. Confirm the typed 6% hit enters 12 ticks of `RESET BOUND`,
    then 30 vulnerable `FORCED GETUP` ticks before idle. Repeat while choosing
    a getup on the collision tick and confirm invulnerability rejects the jab.
    Finally use two hitlag SDI component edges plus away ASDI, confirm the bound
    expires airborne, and use Player 2's aerial instead of forced getup.
48. Use the two jump keys to start a legal air jump, then freshly press light
    or strong attack while `DELAYED AIR JUMP` shows action tick 0–5. Confirm
    the fighter cancels its upward rise, enters the matching aerial, and lands
    earlier than a no-cancel route. Repeat only after the card returns to
    `AIRBORNE` and confirm the aerial retains the full arc. Finally press a
    fresh jump and attack together while ordinarily airborne and confirm the
    attack wins while `air jumps` remains 1.
49. Move both fighters close, jump together, and start Player 1's light aerial.
    Freshly double-jump with Player 2 as the active hit arrives. Confirm Player
    2 takes 8% and freezes in `HITLAG` without a launch, then resumes the same
    `DELAYED AIR JUMP` arc with `air jumps` at 0. Immediately press Player 2's
    light attack and confirm the upward rise cancels into an aerial counter-hit
    on Player 1. Repeat after waiting until Player 2 returns to `AIRBORNE`, then
    repeat against Player 1's strong aerial; confirm the late light and strong
    hits both launch Player 2 normally.
50. Tap and release jump, then press `E` after takeoff. Confirm `PULSE BOLT
    AIR`, a cyan bolt, one typed fire event, and ordinary `LANDING`. Reset and
    fire from the ground to confirm `PULSE BOLT GROUND`. Hold Player 2's shield
    early for the ordinary block, then reset and activate it as the bolt arrives
    during the two-frame window; confirm zero damage, reversed velocity, Player
    2 ownership, a typed reflection event, and the returned hit on Player 1.
51. Keep Player 2 across the stage, fire a fresh Pulse Bolt whenever the prior
    bolt resolves, and confirm the approaching Player 2 remains outside melee
    range while taking repeated projectile hits. Reset, omit Special, and
    confirm the same continuous approach reaches Player 1 and lands a jab.
    This is also the Turtling recipe: the ranged policy avoids contact and
    punishes bad approaches, while the no-projectile control loses that space.
52. Click `Team Wobble Lab`. Use P1 to grab P2, then start P1's down throw and
    P3's fresh light-plus-shield grab together. Repeat with P3 throwing and P1
    waiting; confirm the reciprocal grab owner alternates. Reset the lab, spend
    P3's grab during P1's initial capture, then throw and confirm P2 escapes.

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
- standing and held-grounded-crouch jabs proving equal damage/hitlag, exact 2/3
  launch and hitstun, and a typed crouch-cancel flag in the reaction probe;
- default and 2.0-weight target jabs proving exact two-axis launch halving,
  recomputed lower hitstun, equal damage/hitlag, and default-content restoration
  inside that same reaction probe;
- reduced-stick walking to close, safe, and far attack bands, followed by a
  jab-first responder proving close jab contact, a safe jab-whiff-to-strong
  counter, a far double whiff, and a safe-tip shield block;
- default-stage reduced-stick walking from the full neutral separation into
  the safe whiff conversion, plus an overextended walk being intercepted by
  the same responder;
- floor-to-platform full-hop aerial sharking on the default moving platform,
  plus a too-early active-hitbox whiff and a correctly timed held-shield block;
- ordinary movement into a short-hop neutral-aerial cross-up behind held shield,
  plus an immediate wrong-side whiff and a forward-facing front-block control;
- a combined mindgame gate requiring the jab-read ground conversion,
  shield-read rear cross-up, and both wrong-read outcomes together;
- a grounded launcher into an airborne light-aerial juggle before landing,
  plus a DI-and-directional-air-dodge route that makes the active follow-up
  whiff;
- a full-hop ladder of three upward light aerials, a double jump after hit two,
  and a strong-aerial upper-blast finisher after carrying the target above the
  pass-through platform, plus outward DI making the next active aerial whiff;
- a validated kill-confirm fixture building 120% through legal jabs, linking
  the 126% setup into a 138% typed KO without an actionable defender frame,
  and comparing an 18% non-KO route with a 126% outward-DI active whiff before
  restoring default content;
- the same fixture beginning at exact 0% and chaining 21 earliest-recovery
  jabs into a 138% strong-finisher stock loss without an actionable defender
  frame, plus outward DI after first contact breaking the sequence before the
  finisher while a later active jab whiffs; this uninterrupted prefix also
  supplies the emergent Infinite recipe when the target is wall-pinned and
  jab replaces the finisher after canonical damage saturation;
- a full-hop platform waveland whose retained momentum crosses the support
  edge on the first `SPECIAL LANDING` recovery tick, plus the same ordinary
  inputs at platform center remaining locked for all ten ticks before default
  content is restored;
- initial dash into jump squat and standing grab with retained momentum,
  reciprocal capture state, and a typed grab event, plus direct-dash and
  post-takeoff grab-input rejection before default content is restored;
- ordinary run-to-grab range versus a momentum-preserving boost grab, exact
  dash-attack cancel boundaries, late rejection, and an independently typed
  uncanceled dash-attack hit before default content is restored;
- first-jab shield cancels on hit and whiff at the exact inclusive boundaries,
  early-held and first-late rejection, and the independently typed final jab
  before default content is restored;
- weak-jab contact against vulnerable down wait producing exact 12-tick reset
  bound and 30-tick forced getup, plus same-tick invulnerable getup,
  over-7%-damage rejection, and hitlag-SDI airborne escape before default
  content is restored;
- a complete neutral pummel with exact typed 3% event, retained links, return
  to `GRAB HOLD`, and held-input non-repetition; all four full-direction throws
  with exact typed release events; and a three-down-throw/two-regrab chain
  before default content is restored;
- two alternating four-player team handoffs using the production low down
  throw and fresh allied grab while the victim legally mashes, plus an early
  waiting-grab control; the live lab separately verifies that physical Player
  2 maps to allied simulation slot P3;
- fresh trigger-plus-horizontal input selecting forward/backward roll relative
  to facing, fresh trigger-plus-down selecting spot dodge, and both actions
  reaching their authored invulnerability windows without changing facing;
- two different short-hop release timings producing the same apex;
- two different post-takeoff full-hop hold durations producing the same apex;
- an exact first-airborne-frame instant double jump consuming one air jump and
  applying its authored velocity, plus a held jump through takeoff proving
  edge-triggered non-repeat;
- an early light/strong double-jump cancel during exact action ticks `[0, 6)`,
  a first-late full-arc route, earlier landing after cancellation, and a
  simultaneous jump-plus-attack route that does not consume the air jump;
- a weak aerial striking a fresh delayed air jump, applying damage and hitlag
  while preserving its trajectory/action tick, followed by an immediate
  aerial counter-hit, plus first-late and strong-aerial ordinary-launch
  negatives before default content is restored;
- an aerial Relay Rod drop hitting and bouncing from an aligned target plus a
  spacing miss, grounded glide toss on the final legal roll frame plus the
  first-late rejection, and dash-to-jump-squat cancel throw plus the
  first-airborne-frame ordinary throw before the live item lab is installed;
- an original Pulse Bolt fired from a deterministic short hop, producing the
  aerial fire action/event and returning through generic landing before the
  live item/projectile lab is installed;
- a 180-tick one-slot Pulse Bolt camping loop producing seven legal fires and
  six hits without melee contact, plus a no-fire trace in which the same
  approach-and-jab policy reaches the camper three times;
- an ordinary-input airborne Prism Burst connecting offstage with a downward
  typed hit and causing a stock loss, plus the same victim policy recovering
  when unchallenged before the live reflector lab is installed;
- an Arc Reservoir charge entering store, early shield release into a
  same-tick ordinary attack, exact stored-charge resume, and scaled release
  before the live charge lab is installed;
- an ordinary jump followed by full-up fresh Special entering
  `VECTOR_ASCENT`, consuming the visible once-per-airtime recovery flag, and
  producing the authored positive horizontal and upward velocities before the
  live recovery lab is installed;
- a two-tick reduced-back Moonwalk setup entering a facing-preserving backward
  slide, plus immediate full-back and one-tick-setup dashback negatives;
- a support-edge Teeter entering only after near-edge input release, cancelling
  immediately into standing attack and reverse dash, plus held-outward run-off
  and early-release negatives;
- eight release-gated diagonal-down crouch steps with exact displacement, plus
  held-diagonal, neutral-down, and horizontal-only negative routes;
- a 90-tick grounded Taunt retaining dash momentum and rejecting held
  retriggering, plus support-edge cancellation into `TEETER`;
- an ordinary right-ledge jump reaching the raised-block wall, fresh-away
  entry into `WALL JUMP` with the saved air jump intact, jump/aerial cancels,
  exact authored lock and invulnerability windows, plus an early-away miss;
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
  the exact 60-tick respawn wait, then riding the exact authored revival
  descent, rejecting early input, dropping through ordinary input, and starting
  the exact 120-tick hitbox-rejecting invulnerability timer;
- a real grounded attack producing the configured damage, hitlag, attacker
  identity, and typed ABI-4 hit event; and
- a default strong attack producing 12%, six hitlag ticks, at least 32 hitstun
  ticks, and canonical tumble state;
- an exact 26-tick missed-tech animation entering back-oriented `DOWN WAIT`,
  all three floor-recovery input routes, delayed movement and delayed
  invulnerability for the back/backward roll, and both active phases of the
  floor attack; the native combat oracle covers all four orientation/direction
  schedules;
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
- that shield block crossing one horizontal component for exact 0.66-scaled
  shield SDI, rejecting held and added-vertical repetition, then applying one
  horizontal-only 0.66-scaled shield ASDI shift at shield-stun entry;
- a raw trigger immediately below the light threshold remaining idle, followed
  by exact-threshold light-shield entry with the expected strength and hold
  depletion;
- a physical attack inside the four-tick powershield window producing zero
  shield damage and the powershield result;
- release after that physical powershield preserving the cancel opportunity,
  followed by the one-frame delay and frame-2 ground attack; and
- full held-shield depletion producing the typed system-source break event,
  upward launch, down/stand/stun phase order, fresh-versus-held mash behavior,
  and 30-HP recovery; and
- the native movement/combat oracles covering ledge catch, hang, release, jump,
  climb, roll, attack, simultaneous occupancy, exact option and disabled-regrab
  timing, three hash-equivalent planking refreshes, a mistimed punish,
  mid-climb/mid-roll save-load equivalence, and ledge-attack hitlag rollback.

The page reports
`playtest=ready input_probe=pass air_facing_probe=pass
instant_double_jump_probe=pass double_jump_cancel_probe=pass
double_jump_cancel_counter_probe=pass
bat_drop_probe=pass glide_toss_probe=pass jump_cancel_throw_probe=pass
jump_cancel_probe=pass
edge_hop_probe=pass edge_dash_probe=pass
fox_trot_probe=pass moonwalk_probe=pass teeter_cancel_probe=pass
stage_humping_probe=pass taunt_cancel_probe=pass scar_jump_probe=pass
team_wobble_probe=pass
pivot_probe=pass dash_cancel_probe=pass
dashing_shield_probe=pass shield_platform_drop_probe=pass
small_step_forward_smash_probe=pass
drop_cancel_probe=pass v_cancel_probe=pass approach_probe=pass
spacing_probe=pass sharking_probe=pass cross_up_probe=pass
mindgame_probe=pass juggling_probe=pass ladder_probe=pass kill_confirm_probe=pass
zero_to_death_probe=pass ledge_cancel_probe=pass planking_probe=pass
jump_cancelled_grab_probe=pass boost_grab_probe=pass jab_cancel_probe=pass
jab_reset_probe=pass
chain_grab_probe=pass combat_probe=pass
event_journal_probe=pass reaction_probe=pass
shield_probe=pass shield_break_probe=pass powershield_cancel_probe=pass
tumble_probe=pass
floor_recovery_probe=pass tech_chase_probe=pass surface_tech_probe=pass
air_dodge_probe=pass
ground_dodge_probe=pass
aerial_l_cancel_probe=pass match_probe=pass short_hop_laser_probe=pass
camping_probe=pass
shine_spike_probe=pass charge_storage_probe=pass vector_ascent_probe=pass
gamepad_probe=pass
gamepad_api=available
controls=keyboard-gamepad-two-controller-duel-team-lab
owner_checklist=ready-61` only after all checks pass.
Clean-machine Chrome CI also requires that status and the live playtest DOM.
