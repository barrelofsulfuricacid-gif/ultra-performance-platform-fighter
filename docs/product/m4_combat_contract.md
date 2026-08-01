# M4 combat and hit-reaction checkpoint contract

## Scope

This checkpoint extends the first production-path M4.2 ground attacks with
deterministic hit reaction and the first dense-shield primitive: trajectory
DI, SDI, ASDI, tumble, missed-tech knockdown/down-wait, tech in place,
directional ground tech, reaction-driven tech chasing, wall tech,
wall-tech jump, ceiling tech,
missed wall/ceiling bounce, neutral getup, getup roll, two-sided floor attack,
shield stop, dashing shield, shield platform dropping,
shield damage/stun/pushback, shield release and
regeneration, complete shield-break launch/down/stand/stun/recovery,
physical powershielding, frame-2 powershield canceling into either supported
standing ground attack, and the hitlag-assisted same-platform drop-cancel
route, plus three-frame V-cancelling of eligible airborne launch.
The production dash attack and its three-frame grab-cancel window now compose
with the standing grab to provide ordinary dash grab and boost grab through
the same input, movement, collision, hit, and capture paths.
These primitives use the same normalized input, simulation, save/load, replay,
RL, and browser paths.

Directional air dodge, helpless fall, and momentum-preserving special landing
provide the first defensive-air action and the wavedash/waveland foundation.
Grounded forward roll, backward roll, and spot dodge provide the first locked
grounded escape actions with exact data-defined vulnerability,
invulnerability, motion, and recovery windows.
The first original light aerial now uses the production hit path and composes
with auto-cancel, normal aerial landing lag, exact seven-frame L-cancel timing,
and short-hop-fast-fall-L-cancel. The strong button also works airborne,
reusing the production strong hit data and adding a deliberately conspicuous
30/15-tick landing-lag practice route. This is still an incremental checkpoint. It
does not claim the remaining
attacks, analog light shields, general shield tilt/size/pokes, shield SDI,
pummels and broader throw routes, complete
prone-orientation-specific
getup-roll asymmetry, a moving revival platform, or completion of the 61-row
non-character-specific advanced-technique gate. Configurable stocks, delayed
respawn, respawn invulnerability, elimination, team results, rematch, and
simultaneous-final-stock sudden death are now part of the checkpoint.

## Attack, collision, and ownership

The light or strong ground attack is entered by a rising edge on its separate
input-schema-3 button while the fighter is grounded and outside a locked
action. If both edges occur on one tick, strong attack takes priority. Full
direction plus the light-attack edge also enters the strong ground attack from
idle, providing the standing forward-smash input. Delaying that light-attack
edge for one through three same-direction `INITIAL_DASH` ticks produces the
small-step forward smash: initial-dash travel and retained velocity extend the
same authored hitbox. Frame 4 or a missing full direction produces the ordinary
light ground attack, and an opposite-direction smash has only the one-tick
pivot window. The separate strong button remains a direct strong-attack route. An
airborne light-attack edge instead enters the original light aerial, while an
airborne strong-attack edge enters `STRONG_AERIAL_ATTACK`. There is no
universal input buffer. The default light jab defines two startup ticks, two
active ticks, eight recovery ticks, and four hitlag ticks. The default strong
attack defines five startup ticks, three active ticks, 18 recovery ticks, and
six hitlag ticks. The aerial defines four startup ticks, five active ticks, 23
recovery ticks, and five hitlag ticks while retaining aerial drift, gravity,
and fast fall.

The strong aerial deliberately reuses the strong attack's five-startup,
three-active, 18-recovery, 12%-damage, six-hitlag, launch, and mirrored-hitbox
data. Hitlag therefore resumes the attacker in `STRONG_AERIAL_ATTACK`, not the
grounded strong action.

- Fighter content independently defines each attack's hitbox offset and
  extents, damage, base launch, damage growth, and phase durations.
- The active axis-aligned hitbox is mirrored by facing. The hurtbox is the
  existing data-driven body box.
- Each target can be hit once per attack execution.
- Self-hits and same-team hits are rejected.
- If multiple legal hitboxes overlap one target on the same tick, the lower
  attacker slot owns that target.
- A simultaneous trade resolves both hits. Being hit takes precedence over
  attacker-only hitlag, so each traded fighter resumes in hitstun.

Collision resolves once after every active player completes movement for the
tick, making ownership independent of player step order.

The aerial adds 8% on hit and uses its independent base launch and growth data.
Landing during action ticks 4–24 normally enters 12 ticks of
`AERIAL_LANDING`. A fresh trigger age below seven replaces that with six ticks
of `L_CANCEL_LANDING`; age seven is ineligible. Landing outside the active
window auto-cancels into ordinary `LANDING`. Trigger timing is canonical and
independent from the existing tech window and lockout.

Landing while the strong aerial is active always enters 30 ticks of
`STRONG_AERIAL_LANDING`; an eligible age-0–6 trigger instead enters 15 ticks of
`STRONG_L_CANCEL_LANDING`. These long locked states are an explicit test aid:
the browser renders missed and successful outcomes with red/green banners,
rings, and remaining-frame counts. They do not alter the ordinary light
aerial's 12/6-tick route.

## Drop cancel

The production route follows the documented Melee interaction: start a drop
through a pass-through platform, press light attack on the first airborne
frame, and hit the nearby target. Attacker hitlag continues counting down the
nine-tick pass-through timer. On the final hitlag tick, an exact-timing hit
within the validated five-eighths-unit vertical snap distance returns the
attacker to `AERIAL_LANDING` on that same platform. This is a composition of
the existing platform drop, aerial, hitlag, and landing actions; there is no
drop-cancel-only action or mutable flag. See the
[Drop cancel description](https://www.ssbwiki.com/Drop_cancel).

Starting the aerial one tick late deliberately falls below the snap geometry
even when the attack connects. A frame-perfect whiff has no attacker hitlag to
expire the pass timer and also falls through. The drop-input tick cannot
simultaneously enable fast fall, keeping the authored frame boundary explicit.

## Approach

An [approach](https://www.ssbwiki.com/Approach) is represented by a legal
offensive sequence that closes neutral distance far enough to land an attack.
The simplest supported route starts at the default duel separation and uses
ordinary reduced-stick walking, preserving access to standing attacks at every
step.

The attacker brakes just outside the responder's jab reach. The deterministic
responder jabs; the attacker remains untouched and converts the whiff with the
longer strong attack during recovery. Repeating the same default-stage walk
past that safe band lets the jab intercept the approach before a counter. This
supplies both a successful conversion and a responding-opponent failure case
without scripting positions or outcomes. Dash, aerial, wavedash, shield, and
other independently playable approach primitives remain available for broader
owner testing, but are not conflated with this focused evidence route.

## Camping

The playable [projectile-camping](https://www.ssbwiki.com/Camping) route is a
bounded tactical composition, not a camping-only action or opponent script.
Player 0 holds the safe side of the stage and requests a fresh Pulse Bolt only
when the fixed canonical projectile slot is inactive and grounded fire
recovery has returned to idle. Player 1 continuously supplies full movement
toward the camper and freshly jabs whenever the center distance reaches two
units.

Across 180 production ticks, seven legal fire actions produce six projectile
hits. Player 0 takes zero damage and the minimum center separation remains
693,712 Q16.16 units (about 10.58 world units), so the tactic demonstrably
keeps the active responder outside melee range. The anti-camp control resets
the identical content and opponent policy but omits Special; Player 1 then
closes the gap and lands three ordinary physical hits. Both traces reject an
early match end.

`tests/sim/test_m4_projectile.c` runs both fixed-input policies through
`pf_sim_tick` and exposes their exact bounded counts. Browser startup
independently repeats them in the WebAssembly-facing simulation and exports
`camping_probe` before restoring default content. Existing projectile
save/load, replay verification, structured observation, and compact RL
coverage remain unchanged; broader stages, projectile choices, adaptive
opponents, and owner execution remain before full verification.

The same production trace also supplies the playable
[Turtling](https://www.ssbwiki.com/Turtling) route. That tactic is the broader
defensive composition of avoiding the opponent, using ranged attacks, and
punishing bad approaches. The Pulse Bolt policy supplies all three against a
continuously responding opponent; the no-projectile trace shows the same
approach reaching melee, while the independent Approach/Spacing route proves a
safe whiff punish. Turtling therefore reuses those constituent oracles rather
than adding duplicate state or a tactic-specific test.

## Cross-up

A [cross-up](https://www.ssbwiki.com/Cross-up) times a moving attack so the
attacker passes the opponent and finishes behind them after the hitbox ends.
The focused route uses the documented short-hop back-aerial form. Player 0
first faces away while standing in front of the defender, short hops through
them with ordinary air drift, and preserves that facing through the airborne
route. The production light aerial begins only after player 0 reaches the rear
side, so its backward-facing hitbox contacts the held shield.

The physical block deals zero percent and emits the typed `SHIELD_BLOCK`
event. Because the aerial faces away, normal shield pushback separates the
fighters with the attacker still behind the defender and still facing away
after the hitbox and landing recovery finish. Starting the same back aerial
immediately after takeoff activates it on the wrong side and whiffs. A
forward-facing aerial at the same descent timing blocks but leaves the attacker
in front, providing the side/facing control without scripted repositioning.

The native oracle saves after rear-side aerial startup and compares 48 future
hashes through block and landing. Browser readiness first walks both players to
legal clear-stage space outside the moving platform, then repeats the rear
block, early whiff, and front block through ordinary input. No cross-up-only
state or content field was added; the production grab now makes a shield-grab
comparison possible, but that remains future cross-up fixture coverage.

## Mindgame

The playable [mindgame](https://www.ssbwiki.com/Mindgame) route is a
deterministic bait/read composition, not a new action or a scripted opponent
outcome. Player 0 presents the same visible reduced-stick approach cue while
the responder selects one of two ordinary policies. A jab response is read by
braking outside its reach and converting the recovery with the longer strong
attack. A held-shield response is read by facing away, short hopping through,
and using the rear-side aerial cross-up instead.

The wrong reads remain observable. Committing the ground strong attack into
held shield produces an ordinary physical block rather than damage, while an
immediate back aerial activates on the wrong side and whiffs. The front-facing
aerial control also blocks but stays in front. The startup gate reports
`mindgame_probe=1` only when the approach, close/safe/far spacing, shield
control, rear cross-up, early whiff, and front-block probes all pass together.
Their native save/load continuations cover both the ground counter and aerial
branch. Broader conditioning history and the grab branch remain later fixture
work before this row can become fully verified.

## Juggling

[Juggling](https://www.ssbwiki.com/Juggling) is represented by repeatedly
hitting a target upward without allowing it to recover onto a surface. The
playable route uses only existing match actions: a grounded strong attack
launches for 12%, the attacker follows the airborne trajectory, full hops, and
connects the production light aerial for another 8% while the target is still
airborne. The target's hit sequence changes twice and it never becomes
grounded between the launcher and follow-up.

The negative route holds directional influence through launch, then uses a
fresh directional air dodge as soon as hitstun permits. The attacker performs
the same pursuit and produces an active aerial hitbox, but it whiffs and the
target remains at 12%. This makes the escape policy part of the oracle rather
than a passive target assumption.

The native oracle saves immediately after the launcher connects, reloads, and
compares every future hash through the airborne follow-up. Browser readiness
walks both fighters into legal space on the default stage, waits through
ordinary neutral input for the moving platform to clear each route, and
repeats both the two-hit juggle and DI-plus-air-dodge escape. No juggling-only
action, mutable state, or content field exists. Longer chains, percent and
fighter coverage, stock conversions, owner execution, and complete encoded
replay/rollback evidence remain before the row can become fully verified.

## Ladder combo

A [ladder combo](https://www.ssbwiki.com/Combo#Types_of_combos) links
multiple moves, commonly upward aerials, to carry the defender toward the
upper blast line before a kill move. The playable fixture authors that shape
with existing production fields and actions. Player 0 full hops into three
four-percent light aerials, uses the ordinary double jump after hit two, and
then performs the production strong aerial for the 12% finisher. The sequence
carries Player 1 upward by more than two units, crosses above the pass-through
platform, and takes the stock at 24% through the typed top-blast KO path.

Every follow-up begins on the earliest legal `AIRBORNE` action tick, and the
defender remains in canonical `HITLAG` or `HITSTUN` from the first aerial
through stock loss. The declared defense starts outward DI only after first
contact. Its hitlag displacement and launch influence move the defender out of
the next light aerial: that follow-up becomes active but does not change the
hit sequence, then the defender becomes actionable and survives.

The native oracle saves after the second aerial, loads a second simulation,
and compares every future hash through the strong-aerial KO. Browser readiness
repeats the ladder and DI escape in the WebAssembly-facing simulation before
restoring default content. No ladder-only action, mutable state, or combat
branch exists. Broader fighter and route coverage, owner execution, and
complete encoded replay/rollback evidence remain before full verification.

## Kill confirm

A [kill confirm](https://www.ssbwiki.com/KO_setup) links a relatively fast,
safe setup into a stronger move that takes the stock in a percent range where
throwing out the finisher alone would be riskier. The playable fixture composes
that route entirely from existing production systems. Its data-defined jab has
short recovery and low horizontal launch, while the ordinary strong attack
retains percent-scaled vertical knockback. Twenty legal buildup jabs establish
120%; the setup jab raises the target to 126%, holds it in canonical
hitlag/hitstun without an actionable frame, and the earliest strong follow-up
connects for a typed attacker-attributed KO at 138%.

Two controls establish the useful window rather than merely proving that a
strong attack can KO. From 0%, the same jab-to-strong input reaches only 18%
and the target returns to neutral without losing a stock. At 120%, outward DI
during the setup hit changes the airborne route enough that the same strong
hitbox becomes active but whiffs, leaving the target at 126%. This DI-dependent
escape is expected for this fixture and remains visible in the oracle.

The native test saves immediately after the high-percent setup connects, loads
the state into a second simulation, and compares every future hash through the
KO event. The browser startup probe initializes the same validated content,
runs the high-percent conversion and both negative routes through ordinary
input, then reinitializes the default content before exposing the live page.
No kill-confirm action, mutable state, or special-case combat branch exists.
Broader fighter/percent windows, owner execution, and encoded replay/rollback
coverage remain before the row can become fully verified.

## Infinite

The playable [infinite](https://www.ssbwiki.com/Infinite) is an emergent branch
of the existing fast-jab fixture. Place the defender against the fixture's
solid wall and restart the ordinary jab on each earliest grounded-idle frame.
The authored near-zero knockback growth and wall collision prevent separation;
after the canonical 999% damage cap is reached, the same attack/hitlag/hitstun
cycle can continue without a changing damage resource. No infinite-only state,
action, or outcome exists.

The existing zero-to-death oracle supplies a 21-jab uninterrupted prefix,
mid-chain save/load equality, and browser-native execution. Its open-stage
outward-DI branch makes a later active jab whiff, providing the input/geometry
change that breaks the loop. A longer repeated-state trace and owner execution
remain before the registry row can advance beyond `playable`.

## Zero-to-death combo

A [zero-to-death combo](https://www.ssbwiki.com/Zero-to-death_combo) begins
with the defender at exactly 0% and ends in a KO without an interruption. The
playable route reuses the validated fast-jab fixture but does not use the
kill-confirm route's neutral buildup. Player 0 starts each next jab on the
earliest grounded-idle tick; 21 six-percent jabs carry Player 1 continuously
from 0% to 126%, then the earliest ordinary strong finisher takes the stock at
138%. After the first hit, every pre-KO inspection keeps the defender in
canonical `HITLAG` or `HITSTUN`.

The declared defense policy begins outward DI only after the first hit makes
contact. Its hitlag displacement and launch influence break the chain before
the strong finisher: a subsequent ordinary jab produces an active hitbox that
whiffs, the defender becomes actionable, and no stock is lost. This bounded
finisher branch ends in a stock conversion and does not itself claim to be the
wall-pinned infinite described above.

The native oracle starts from exact zero damage, saves immediately after hit
11, loads a second simulation, and compares every future hash through the
typed attacker-attributed KO. Browser readiness repeats both the uninterrupted
conversion and outward-DI escape in the same WebAssembly-facing simulation,
then restores default content before exposing the live page. No
zero-to-death-only action, mutable state, or special-case combat branch exists.
Broader fighter coverage, owner execution, and complete encoded
replay/rollback evidence remain before the row can become fully verified.

## Spacing

[Spacing](https://www.ssbwiki.com/Spacing) is treated as a tactical composition,
not a technique-only action: the player must judge an opponent's timing and
range, avoid the option, and counter it through ordinary match input. The
default fighter's forward jab reaches 1.8 center-to-center units, while its
forward strong attack reaches 2.1 units.

The repeatable responder policy jabs first. At 1.95 units the jab becomes
active without touching the opponent, who begins a strong counter while the
jab is still active; the longer hitbox connects during jab recovery. At 1.7
units the same jab hits before the counter can start. At 2.25 units both the jab
and strong counter whiff. A separate 1.95-unit shield route proves that the
strong attack still makes legal contact at the tip rather than passing through
the target.

The native oracle uses ordinary button input from valid duel spawns, saves
after the whiff-counter has begun, reloads, and compares 32 future hashes. The
browser startup probe reaches the same close, safe, and far bands through
reduced-stick walking on the default stage, then repeats the jab-first policy
and the shield control. No mutable state, action, or content schema was added.

## Sharking

[Sharking](https://www.ssbwiki.com/Sharking) is represented as an attack on a
platform opponent from underneath the stage surface. The focused route uses
the existing one-way platform collision: the target stands on top while the
attacker remains in legal floor space below it, full hops upward, and begins
the production light aerial before crossing the platform line. The attacker
may then pass through the semisoft platform while the active hitbox connects.

The positive route deals the authored 8% aerial damage and records player 0 as
the attacker. Starting the same aerial immediately after takeoff activates its
hitbox too far below the target and whiffs, proving the hit is geometric rather
than scripted. Holding the target trigger produces a normal shield block with
zero percent, reduced shield health, no powershield, and a typed
`SHIELD_BLOCK` event, so platform sharking also supplies real shield pressure.

The native oracle saves immediately after below-platform aerial startup,
reloads, and compares 32 future hashes through the hit. Browser readiness
repeats the hit, early-whiff, and held-shield routes on the default moving
platform, using ordinary floor walking and full airborne steering to track its
motion. No sharking-only action, state field, or content data exists.

## V-cancelling

The production route follows the documented Melee defensive input: fully press
a trigger on the collision tick or either of the preceding two ticks while in
an eligible airborne state. `AIRBORNE`, `FALL_SPECIAL`, and vulnerable
`AIR_DODGE` startup are eligible. Grounded states, active aerial attacks, and
locked hitstun are excluded; post-hitstun tumble is represented by the eligible
`AIRBORNE` action in this simulation.

The qualifying trigger edge must also be the edge that opened the existing
40-tick tech lockout. Releasing and pressing again during that lockout updates
the visible input age but cannot V-cancel. This composes the canonical trigger
age and tech-lockout fields without adding a V-cancel action or mutable flag.

A successful V-cancel multiplies both pending launch components by the
data-defined Q16.16 scale, 95% by default. Hitstun and tumble are computed from
the ordinary launch first and remain unchanged. The typed hit event reports the
scaled vector, so save/load, replay, browser, and verifier paths observe the
same deterministic result. See the
[V-cancelling description](https://www.ssbwiki.com/V-cancelling).

## Double jump cancel counter

The placeholder fighter authors a 20-tick maximum hitstun threshold for
knockback-based armor during `DELAYED_AIR_JUMP`. Hit resolution first computes
the ordinary physical-hit reaction. If the defender was in that delayed jump
and the computed hitstun is nonzero and at or below the authored threshold,
the hit still applies damage, attribution, and ordinary hitlag to both players,
but applies no launch, hitstun, or tumble. The hit event remains the ordinary
typed physical hit with zero velocity and no reaction flags.

The defender's position, velocity, air-jump consumption, delayed-jump action
tick, and cancel window freeze through hitlag. When hitlag ends, the defender
resumes `DELAYED_AIR_JUMP` at that same action tick and may immediately use the
ordinary light or strong aerial cancel. This preserves the original
double-jump trajectory until the counter aerial cancels its upward momentum;
it does not add a counter-only action, input, event, or mutable state.

The threshold is inclusive. The focused fixture's 16-hitstun aerial qualifies
at a threshold of 16 and launches normally at 15 or zero. A hit after the
six-tick delayed-jump window and the fixture's 34-hitstun strong aerial both
use the ordinary launch/tumble path. Non-physical reactions never qualify, and
an armored hit neither consumes nor activates V-cancel. The behavior follows
the knockback-armor and immediate-aerial counter topology documented for
[double jump cancel counter](https://www.ssbwiki.com/Double_jump_cancel_counter),
while all timing and fighter data remain original placeholders.

## Damage, hitlag, launch, and hitstun

Damage is unsigned Q16.16 percent and saturates at 999%. The default light jab
adds 6%; the default strong attack adds 12%. Launch uses post-hit damage:

- horizontal launch is facing-signed base X plus damage-scaled growth;
- vertical launch is upward base Y plus half the damage-scaled growth; and
- validation and runtime saturation keep both components inside the canonical
  motion-speed bound.

On impact, attacker and target enter the selected attack's data-defined hitlag
(four ticks for light, six for strong). The target retains a pending launch
vector and hitstun duration, then becomes airborne in `HITSTUN`. Hitstun is the
ceiling of the sum of absolute launch components divided by the data-defined
velocity per tick, clamped to 1–600 ticks.

A target enters tumble when that computed hitstun reaches the data-defined
threshold, 32 ticks by default. The default light jab remains below that
threshold on a fresh fighter; the default strong attack exceeds it on its first
hit, providing a direct tumble test path. Jump, attack, fast fall, and ordinary
steering remain locked during hitstun; deterministic gravity and stage
collision continue.

## DI, SDI, and ASDI

The hit target can affect its reaction through the normalized main stick:

- SDI reads each hitlag tick. Crossing the 0.5-axis threshold into a new
  horizontal or vertical component applies a 0.3-unit normalized positional
  shift. Holding the same direction does not repeat a pulse; adding the second
  component of a diagonal does.
- ASDI applies one 0.15-unit normalized positional shift from the final hitlag
  input.
- Trajectory DI reads the final hitlag input and rotates pending launch toward
  the stick's perpendicular component. Full perpendicular input reaches the
  data-defined 18-degree maximum. Parallel input produces no rotation.
- The fixed-point DI path uses deterministic integer square root and
  renormalizes the rotated vector to preserve launch speed within integer
  truncation.
- A hitlag shift cannot pass downward through a floor or pass-through
  platform. A grounded shift can move upward or beyond a support edge and
  become airborne.

The attacker does not receive target SDI/ASDI/DI behavior from attacker-only
hitlag.

## Tumble landing and ground tech

Either analog trigger at or above the data-defined digital threshold is the
tech input. A rising edge opens a 20-tick tech window and starts a 40-tick
lockout. Holding the trigger does not reopen the window; it must be released
and pressed again after lockout.

When a tumbling fighter contacts a floor or pass-through platform:

- an open window plus neutral horizontal input enters `TECH_IN_PLACE` for 26
  ticks;
- an open window plus horizontal input enters `TECH_ROLL` for 40 ticks in that
  direction at the data-defined roll speed; and
- no open window enters a vulnerable 26-tick `KNOCKDOWN`, followed by
  `DOWN_WAIT`.

The placeholder now uses Melee's universal 26-tick tech-in-place and 40-tick
tech-roll durations; both reject hits for their first 20 ticks and expose that
derived invulnerability through inspection. `KNOCKDOWN` and `DOWN_WAIT`
remain vulnerable. Successful techs return directly to ground idle.

After the missed-tech animation, ordinary match inputs select the floor
recovery:

- up or a fresh shield edge enters a 30-tick `GETUP_NEUTRAL`, invulnerable for
  its first 23 ticks;
- left or right enters a 35-tick `GETUP_ROLL` in that direction, invulnerable
  for its first 19 ticks;
- either attack edge enters a 49-tick `GETUP_ATTACK`, invulnerable for its
  first 26 ticks; its 6% hitbox attacks in front on frames 17–19 and behind on
  frames 24–26; and
- 180 inactive `DOWN_WAIT` ticks automatically select neutral getup. This
  timeout is original placeholder content rather than a claim of one
  universal Melee character value.

The current one-fighter placeholder has one getup-roll timing table.
Orientation-specific backward-roll differences remain explicit fighter-data
work. The timing contract follows the
[Melee tech frame-data summary](https://www.reddit.com/r/smashbros/comments/1svuas/when_is_it_possible_to_hit_an_opponent_who_missed/)
alongside the 20-frame input window and 40-frame lockout described by
[SmashWiki](https://www.ssbwiki.com/Tech). Floor input choices and the
two-sided weak attack follow SmashWiki's
[floor-getup](https://www.ssbwiki.com/Floor_getup) and
[floor-attack](https://www.ssbwiki.com/Floor_attack) descriptions.

## Reaction-driven tech chase

The playable tech-chase route is a composition of production mechanics, not a
new scripted action. The attacker follows the target while the target is still
airborne, observes the ground recovery selected by ordinary target input, and
continues moving toward that outcome. Neutral input yields `TECH_IN_PLACE`;
right input yields `TECH_ROLL` with direction `+1`.

The chaser may start a standing jab only after the target action timer reaches
the exact 20-tick tech-invulnerability boundary and only while the mirrored jab
can reach the observed position. Jab startup must still complete before the
26-tick in-place or 40-tick roll action ends. A non-reacting comparison stays
at its original spacing and presses jab at the same 20-tick boundary; the roll
escapes and takes no additional damage. This follows
[SmashWiki's tech-chasing description](https://www.ssbwiki.com/Tech-chasing)
of reacting to a floor-recovery option and punishing its vulnerable ending.

Canonical save/load remains part of the route: saving during the invulnerable
part of the right roll, loading into a second simulation, and supplying the
same subsequent chase inputs must produce equal future hashes through the jab
hit.

## Wall, ceiling, and missed surface impacts

The default stage adds one raised solid block. Its top is a normal landing
surface; its sides and underside resolve body collision. Only a tumbling
fighter can enter the surface-tech or missed-bounce actions:

- An open tech window on a side impact enters `WALL_TECH`, faces away from the
  wall, clears hitstun/tumble/window state, stalls for three ticks, and then
  moves away at the data-defined 0.15 speed.
- A fresh jump edge or held up input on that same successful impact selects
  `WALL_TECH_JUMP`; after the same stall it launches 0.30 away and 0.50 upward.
- An open tech window on the underside enters `CEILING_TECH`, clears
  hitstun/tumble/window state, zeros vertical velocity, and applies horizontal
  stick input up to the data-defined 0.16 speed.
- Missing the window enters `WALL_BOUNCE` or `CEILING_BOUNCE`. The impact
  reflects the surface-normal component, multiplies both motion components by
  the data-defined 0.8 coefficient, and preserves tumble and remaining
  hitstun.

Wall tech actions last 24 ticks and ceiling tech lasts 30. All successful
surface techs use the existing exact 20-tick recovery-invulnerability rule.
These placeholder velocities and action durations are data, while the
transition structure follows the wall/ceiling passive checks and reflected
damage-flight path in the pinned Melee decomp:
[wall passive](https://github.com/r-burns/doldecomp-melee/blob/96dadb63c038c81e3a792e04d2b20fe91ce5a983/src/melee/ft/chara/ftCommon/ftCo_PassiveWall.c),
[ceiling passive](https://github.com/r-burns/doldecomp-melee/blob/96dadb63c038c81e3a792e04d2b20fe91ce5a983/src/melee/ft/chara/ftCommon/ftCo_PassiveCeil.c),
and
[flight reflection](https://github.com/r-burns/doldecomp-melee/blob/96dadb63c038c81e3a792e04d2b20fe91ce5a983/src/melee/ft/chara/ftCommon/ftCo_FlyReflect.c).

## Dense shield, shield stop, and release

Either normalized analog trigger at or above the fighter's digital threshold
raises a full-density shield on frame 1 from supported grounded actionable
states. Initial dash is deliberately excluded; the fighter must first reach
run or another shieldable grounded state. Raising shield from run preserves
horizontal momentum and applies normal traction each tick, producing the
forward slide required for shield stop.

The current full-density values are data, not hidden constants:

- 60 shield HP, 0.28 HP depletion per held tick, and 0.07 HP regeneration per
  non-shield tick;
- an eight-tick minimum hold before release and 15 ticks of ordinary shield
  release lag;
- immediate jump cancel from an already active shield or its release state;
  and
- reset to 30 HP after shield-break stun ends or a flinching punish
  interrupts it.

Shield release regenerates health because the blocking volume is no longer
active. Holding shield cannot reopen a tech window without a new trigger edge,
so the existing tech-window/lockout contract remains intact.

A dashing shield is the one-tick tap/release form of the same production
run-to-shield transition. It inherits run momentum and follows the exact shield
stop traction path through the eight-tick minimum hold, then enters the
15-tick release instead of remaining shielded. A held comparison therefore has
the same position and velocity but a different action state at the minimum-hold
boundary; an idle tap has no horizontal travel.

## Blocking, shield stun, and powershield

Each current physical attack intersects the grounded fighter body box as
the current shield collision volume. A legal block:

- prevents percent gain and launch;
- applies ordinary hitlag to attacker and defender;
- applies shield damage equal to base damage multiplied by 0.7;
- floors shield stun from `(damage * 0.45 + 2) * 200 / 201`;
- applies defender pushback `(damage * 0.09 + 0.4) * 0.6`, capped at 2; and
- applies attacker pushback `damage * 0.07 + 0.02`.

The defender resumes in `SHIELD_STUN` after hitlag and cannot act until its
timer expires. Holding the trigger then returns to shield; releasing it enters
ordinary shield-release lag.

A physical hit during the first four active shield ticks is a powershield. It
takes no shield damage, retains the same hitlag and shield stun as an ordinary
Melee physical block, and uses the larger defender pushback factor of 1. The
powershield result flag remains inspectable through hitlag and shield stun.
If shield is still held when stun ends, the flag clears and the fighter returns
to ordinary shield. Releasing before stun ends instead carries the flag into
shield drop and opens the cancel path described below. Projectile reflection
remains separate work.

These values follow the Melee dense-shield and pushback tables in
[SmashWiki's shield reference](https://www.ssbwiki.com/Shield), the four-frame
physical window and no-damage behavior in its
[powershield reference](https://www.ssbwiki.com/Power_shield), and the
traction-preserving input sequence in its
[shield-stop reference](https://www.ssbwiki.com/Shield-stop).

## Physical powershield cancel

After a physical powershield, releasing shield by the end of shield stun enters
`SHIELD_RELEASE` with the cancel opportunity intact. The content table defines
the one-tick delay and whether the fighter supports the technique.

- Either attack on frame 1 of shield drop is rejected.
- A fresh light- or strong-attack edge on frame 2 or later cancels directly
  into the selected production action.
- An attack pressed too early is not buffered; it must be released and pressed
  again on a legal frame.
- Holding shield through the end of shield stun consumes the opportunity.
- An ordinary physical block never receives this cancel and still pays the
  complete 15-tick shield-release duration.
- Jump remains an ordinary out-of-shield cancel and does not depend on a
  powershield.

This implements the Melee physical timing documented by
[SmashWiki's powershield-cancel reference](https://www.ssbwiki.com/Powershield_canceling):
ground attacks begin on frame 2 of shield drop, after one frame of delay.
The registry remains conservative at `playable` until every future supported
ground action routes through the same cancel and receives positive/negative
coverage.

## Shield break checkpoint boundary

Reaching zero HP, either from holding shield or blocking the current attack,
enters the complete deterministic shield-break route:

1. `SHIELD_BREAK` clears support and launches straight upward at the
   content-defined speed. Input cannot steer, fast-fall, reverse facing, grab a
   ledge, or otherwise cancel this flight.
2. Gravity and normal stage collision continue. The first legal floor,
   platform, or solid-top contact enters `SHIELD_BREAK_DOWN`; holding down
   cannot pass through a platform during this forced landing.
3. The original placeholder fighter spends 30 data-defined ticks down and 30
   standing before entering `SHIELD_BREAK_STUN`.
4. Stun begins with
   `max(90, 490 - floor(current_percent))` remaining ticks. Each fresh jump,
   light attack, strong attack, trigger, full-horizontal flick, or down flick
   removes three additional ticks; holding an input does not retrigger it.
5. Flight, down, and stand reject hitboxes. Stun is vulnerable. A flinching
   hit immediately leaves stun through ordinary hitlag/hitstun and restores
   the shield to 30 HP; natural stun expiry returns to idle with the same
   reset.

The phase order and stun equation follow Melee's documented
[shield-break sequence](https://www.ssbwiki.com/Shield#Shield_breaking) and
[dazed-duration formula](https://www.ssbwiki.com/Dazed). The launch speed and
down/stand animation durations remain explicit original-fighter content, not
hidden engine constants. If a moving support leaves a downed, standing, or
stunned fighter unsupported, the fighter returns to locked
`SHIELD_BREAK` fall and repeats the landing phase.

Both hit-caused and hold-depletion breaks emit the typed shield-break event.
The latter uses source `255` (system/no player), the actual depleted health as
its value, the launch vector as velocity, and detail zero.

## Air-dodge hit interaction

The air-dodge action's exact data-defined invulnerability window is resolved
by the same production hit-ownership pass as tech/getup invulnerability.
Default action ticks 3–28 reject physical hitboxes; startup ticks 0–2 and tick
29 onward accept them. Inspection derives the visible invulnerability marker
from the action ID, timer, and fighter data, so it adds no redundant mutable
flag. Hits outside the window replace the dodge with ordinary hitlag and
hitstun.

## Grounded dodge and roll hit interaction

`ROLL_FORWARD`, `ROLL_BACKWARD`, and `SPOT_DODGE` use the same production
hit-ownership pass as tech, getup, and air-dodge invulnerability. The current
roll table rejects physical hitboxes on half-open action ticks `[4, 17)`;
startup ticks 0–3 and tick 17 onward accept hits. Spot dodge rejects hits on
`[3, 16)` and is vulnerable outside that range.

Inspection derives the gold invulnerability marker from action ID, action
timer, and fighter data. A hit outside the legal window replaces the defensive
action with ordinary hitlag and hitstun; there is no redundant mutable
invulnerability flag.

## Stocks, respawn, and match result

`pf_sim_config` carries the deterministic match rules. The default is four
stocks, a 60-tick respawn wait, and 120 ticks of post-spawn invulnerability.
Stock count accepts 1–99; zero is the explicit unlimited-stock practice mode
used by legacy movement/combat traces. Both timers accept 0–3600. A zero
respawn delay still spends one canonical tick in `RESPAWN_WAIT`, while zero
invulnerability makes the fighter hittable on its spawn tick.

Crossing a blast boundary increments the bounded respawn count and consumes
one stock. A fighter with stocks remaining becomes inactive in
`RESPAWN_WAIT`, then reappears at its deterministic stage spawn with damage,
reaction, shield, attack, and per-life last-hit state reset. The configured
invulnerability timer rejects production hitboxes without locking movement or
attacks. A fighter with no stock enters `ELIMINATED` and no longer participates
in collision. This checkpoint returns the fighter directly to the authored
ground spawn after the wait; a moving revival-platform presentation and its
drop-off interaction remain later M4 stage work.

The match terminates as soon as only one team retains any stock. `winner_mask`
contains every slot on that team, including an eliminated teammate. If all
teams lose their final stock on the same tick, the match instead enters a
deterministic sudden-death fixture: all players receive one stock, wait for the
configured respawn delay, and return at 300%. If every team is again KO'd on
the same sudden-death tick, the lowest controller port wins; in teams, that
port's complete team mask wins. This follows Melee's simultaneous-final-stock
sudden-death rule and deterministic port tie resolution while avoiding an
unbounded repeat.

Stocks, timers, sudden-death state, and match result are observable through
the structured simulation, M4 inspection, RL compact, save/load, replay,
native, and browser paths. They also enter the configuration hash, so snapshots
and replays cannot load under different match rules.

## Ledge regrab and planking

An ordinary jump, down release, or outward release from `LEDGE_HANG` starts a
data-defined 29-tick disabled-regrab period. The release frame is already
excluded from catch resolution; the timer then decrements before each later
catch attempt, so remaining ticks 28 through 1 reject an otherwise legal
catch and remaining tick 0 permits it. Facing, vertical direction, catch
volume, and single-occupancy rules still apply. This timer is separate from
the pass-through-platform timer and therefore never suppresses floor or
platform landing. The timing and repeated drop/regrab route follow the
documented [ledgestall](https://www.ssbwiki.com/Ledgestall) and
[planking](https://www.ssbwiki.com/Planking) behaviors.

A legal catch refreshes the independent 37-tick ledge-invulnerability timer.
The planking fixture tunes only the ordinary double-jump arc and the
responding opponent's jab reach: three drop/double-jump cycles return to the
catch volume on the first legal tick and reject the active punish after each
refresh. Fast-falling on the final two ticks leaves the catch volume, lets
invulnerability expire, and accepts that same punish. The native oracle saves
immediately after release and compares every future canonical hash through all
three regrabs; browser startup repeats both outcomes and restores default
content.

The same bounded production route satisfies playable
[stalling](https://www.ssbwiki.com/Stalling) evidence without adding a
stalling-only action or test path: repeated safe refreshes deliberately delay
engagement, consume and restore the air-jump/protection resources, and the
existing mistimed route proves that the attacking opponent can punish its
vulnerability boundary.

## Grab, capture, escape, and jump-canceled grab

A fresh light-attack edge while a shield trigger is held selects the standing
grab from idle, walk, crouch, shield, `RUN`, or `JUMP_SQUAT`. It does not select
grab from `INITIAL_DASH` or an airborne action. A direct selection from `RUN`
is the ordinary dash grab. The jump-canceled advanced route instead begins in
initial dash, enters jump squat with jump, and selects grab on the next tick.
The standing grab preserves inherited horizontal velocity and applies normal
traction, matching the documented
[jump-canceled-grab](https://www.ssbwiki.com/Jump-canceled_grab) behavior of
interrupting dash with jump and entering standing grab during jump startup.
No technique-only input bit or action exists; direct initial-dash
light-plus-shield and post-takeoff light-plus-shield are explicit negative
routes.

The authored grab has four startup ticks, two active grabbox ticks, and ten
recovery ticks. Capture is deterministic by controller port, bypasses shield,
rejects invulnerable or same-team targets, clears incompatible attack/reaction
state, and links one attacker to one victim. `GRAB_HOLD` tethers the victim at
the authored offset while `GRABBED` suppresses normal movement and attacks.
The initial escape timer is 30 ticks plus one tenth of a tick per damage
percent, capped at 90. It decrements naturally once per tick; each fresh
button, full-horizontal, or full-down edge removes three additional ticks,
while held input never repeats the reduction. Natural or mash escape emits a typed event,
clears both reciprocal links, and gives both fighters eight ticks of
`GRAB_RELEASE`.

The native oracle covers shielded and ordinary capture, exact active frames,
natural and mash escape boundaries, invulnerable spot-dodge rejection,
direct-initial-dash and airborne negatives, retained dash momentum, typed
events, and
mid-hold save/load with equal future hashes. Browser startup repeats the
jump-cancel route and both negatives; browser view schema 18 retains the cyan
grabbox, `GRAB`/`GRAB HOLD`/`GRABBED`/`GRAB RELEASE`, reciprocal owner/target
links, and the victim's `MASH OUT · Nf` countdown.

## Jump-cancelling attack

During the production three-tick jump squat, full up plus a fresh light or
strong attack selects the existing grounded `STRONG_ATTACK`, cancels takeoff,
and preserves inherited dash velocity before ordinary traction. This is the
attack branch of the documented
[jump-cancel](https://www.ssbwiki.com/Jump_cancel) pre-jump-lag router; the
existing light-plus-shield grab and held-item throw are its grab and item
branches. It adds no technique-only action, mutable flag, or input bit.

Neutral stick or an up magnitude one unit below the full-input threshold keeps
the fighter in jump squat. Waiting until the first airborne frame selects the
ordinary aerial attack instead. The native oracle covers both attack buttons,
both grounded exclusions, retained dash momentum, the late aerial route, a
real 12% strong hit, and a mid-action save/load whose remaining state hashes
and events match. Browser startup repeats both positive routes and all three
negative controls before readiness; browser view schema 23 carries that new
readiness semantic without changing the 290-value view layout.

## Dash attack and boost grab

A fresh light-attack edge from `RUN` enters the production `DASH_ATTACK` and
sets horizontal velocity to the authored `7/20` unit-per-tick speed before
ordinary fighter traction. Its independent forward hitbox deals 8%, has four
startup ticks, three active ticks, 12 recovery ticks, and five hitlag ticks,
and emits the ordinary typed `HIT` event with `DASH_ATTACK` in `detail`.
Damage, signed launch, hitbox geometry, speed, phase durations, and hitlag are
all hashed and validated fighter content rather than technique-only fixtures.

The documented [boost grab](https://www.ssbwiki.com/Boost_grab) route adds a
fresh shield while the attack button remains held, or a fresh light attack
while shield remains held, during stored action ticks 1–3. Those inputs are
the second through fourth dash-attack frames: the initiation frame cannot
cancel. The legal cancel enters the existing standing `GRAB` without replacing
the dash-attack velocity, so the faster forward slide reaches a target that
the ordinary run-to-grab route misses. Light plus shield together on the
initial `RUN` tick selects the ordinary dash grab, not boost grab. Input after
the stored tick-3 boundary leaves `DASH_ATTACK` intact.

The native oracle compares exact ordinary and boosted velocities and active
positions, requires the ordinary range whiff and boosted capture, rejects the
late cancel, independently proves the dash attack's first active-frame damage
and typed identity, rejects invalid speed/cancel data, and saves on stored
dash-attack tick 1 before comparing every future hash and event through
capture. Browser startup repeats the ordinary, boost, late, and dash-hit routes
before exposing readiness; the live adapter labels the production action
`DASH ATTACK`.

## Jab sequence and jab cancel

The production neutral attack is now a two-hit decision sequence. The existing
`GROUND_ATTACK` is the first jab: two startup ticks, two active ticks, eight
recovery ticks, 6% damage, and four hitlag ticks. Stored action ticks 4 through
7 form the hashed and validated combo-input window. A fresh shield trigger in
that inclusive window cancels directly into the existing production `SHIELD`
state on either hit or whiff. A fresh light-attack edge instead selects the
independently authored `JAB_FINAL`; neutral input lets the first jab finish.
This follows the documented Melee behavior in which
[jab cancelling](https://www.ssbwiki.com/Jab_cancelling) uses shield to stop a
neutral-attack sequence before its final hit. An early trigger held into the
window never becomes fresh, and the first frame after the stored tick-7
boundary cannot cancel.

`JAB_FINAL` has its own forward hitbox, 7% damage, signed base launch and
per-percent growth, two startup ticks, two active ticks, ten recovery ticks,
and four hitlag ticks. It emits the ordinary typed `HIT` event with
`JAB_FINAL` in `detail`. The transition resets the shared per-attack hit mask,
so the same target can legally receive the independently identified final hit;
no jab-cancel flag, buffered choice, or other technique-only mutable state is
added.

The native oracle proves shield cancel on hit at the exact opening boundary,
shield cancel on whiff at the exact closing boundary, early-held and first-late
rejection, final-hit damage and typed identity, invalid window/final-hit data,
and a mid-window save/load comparison of every future hash and event. Browser
startup repeats the hit, whiff, both negative boundaries, and final-hit routes,
restores default content, exposes `jab_cancel_probe=pass`, and labels the live
production action `JAB FINAL`.

## Jab reset

The production [jab-reset/lock](https://www.ssbwiki.com/Spooky_stun) route
begins only when a physical hit reaches a vulnerable target already in
`DOWN_WAIT` or `RESET_BOUND`. The hit qualifies when its independently authored
damage is no more than `reset_max_damage_q16` (7% by default) and its computed
hitstun is no more than `reset_max_hitstun_ticks` (12 by default). Both limits
are inclusive. A hit above either limit follows the ordinary hit-reaction path;
the 13-hitstun boundary tumbles under the focused fixture rather than silently
becoming a reset.

A qualifying hit still emits the ordinary typed `HIT`, applies damage and
hitlag, and accepts the shared SDI, ASDI, and final-hitlag DI inputs. Its
reaction clears horizontal launch, applies the small authored upward bound
speed of `1/10` unit per tick, clears tumble, and resumes from hitlag into
`RESET_BOUND`. The existing action timer governs exactly 12 bound ticks. If the
target has support when those ticks expire, it enters `FORCED_GETUP` for 30
vulnerable ticks; if still airborne, it returns to ordinary `AIRBORNE` and may
act immediately. Attack and jump input are locked during the bound and forced
getup, and neither action grants invulnerability.

Movement and getup choices resolve before hit ownership. A same-tick neutral
getup or getup roll therefore uses its existing invulnerability to reject the
jab. During reset hitlag, two legal SDI pulses plus ASDI can move the target far
enough to remain airborne at bound expiry and use an aerial instead of entering
forced getup. This preserves the documented escape routes without adding a
reset flag, forced-getup counter, or other technique-only mutable field.

The native oracle proves the default authored values, typed 6%/12-hitstun
route, exact 7%/12-hitstun inclusive boundary, over-damage and tumble
rejection, exact bound/getup duration and input lock, same-tick invulnerable
getup, airborne SDI escape, invalid content, and a save on bound tick 3 followed
by 64 equal future hashes and events through a real forced-getup punish.
Browser startup repeats the positive, getup, over-damage, and SDI routes,
restores default content, exposes `jab_reset_probe=pass`, and labels
`RESET BOUND` and `FORCED GETUP`.

## Directional throws and chain grab

During `GRAB_HOLD`, a fresh light or strong attack plus a full stick direction
selects one of `THROW_FORWARD`, `THROW_BACK`, `THROW_UP`, or `THROW_DOWN`.
Horizontal direction is resolved relative to facing. A strictly larger vertical
magnitude selects up/down; horizontal wins an exact diagonal tie. Neutral or
reduced direction plus attack remains `GRAB_HOLD`, so no hidden neutral throw
or presentation-only shortcut exists.

Each throw is authored independently:

| Throw | Damage | Base launch (facing x, y) | Per-percent growth (facing x, y) | Release | Hitlag | Recovery |
|---|---:|---:|---:|---:|---:|---:|
| Forward | 8% | `(+1/4, -9/50)` | `(+1/512, -1/1024)` | tick 3 | 4 | 12 |
| Back | 9% | `(-3/10, -4/25)` | `(-1/512, -1/1024)` | tick 4 | 4 | 14 |
| Up | 7% | `(+3/100, -9/25)` | `(+1/4096, -1/512)` | tick 3 | 4 | 11 |
| Down | 6% | `(+1/25, -2/25)` | `(+1/512, -1/2048)` | tick 2 | 3 | 5 |

The victim remains tethered by reciprocal grab links through the authored
startup. On the exact release tick, the simulation computes launch from the
post-damage percent, clears both links atomically, and applies the shared
physical-hit reaction path, including hitlag, hitstun/tumble, SDI, final-hitlag
DI, attribution, and deterministic event sequencing. Both fighters freeze for
the authored hitlag; the thrower resumes the same throw action for its authored
recovery. The event is `THROW`, with throw action in `detail`, applied damage in
`value`, and the pre-DI authored launch vector in the velocity fields.

Chain grab is an emergent consequence of those production rules. The default
down throw leaves a low-percent victim close enough for pursuit and two legal
regrabs, producing three throws and 18% total damage. After two ordinary
45%-damage setup hits, the same down throw reaches 96%; outward DI/SDI moves the
victim beyond the earliest standing regrab, whose active grabbox whiffs without
a new link or event. The native oracle also saves during the second down-throw
startup with live reciprocal links, loads it, and compares every future hash
and event through the remaining throws and regrabs. Browser startup executes
all four directional throws, the complete low-percent chain, and the neutral
input negative before restoring default content; the live adapter exposes all
four action labels and the typed throw event.

## Relay Rod item contract

The original Relay Rod is one fixed-capacity canonical entity. Default content
keeps it disabled so established combat fixtures remain isolated; the browser
lab deliberately enables it at its authored `x=-7` ground spawn. No pickup,
throw, collision, despawn, or reset path allocates memory or creates a dynamic
entity.

The item state machine is `INACTIVE`, `GROUND`, `HELD`, `AIRBORNE`, and
`RESPAWN_WAIT`. Its data defines pickup extents, held offset, gravity and fall
speed, four directional throw vectors, 3/4 momentum transfer, an axis-aligned
airborne hitbox, 7% damage, knockback growth, hit bounce, pickup lockout,
600-tick ordinary lifetime, and 60-tick respawn. The long-lived browser lab
uses the same content with a 3,600-tick lifetime so the practice object does not
reset during an ordinary session.

Grounded light plus shield picks up a nearby ground item. While held, fresh
light or strong attack throws in the selected forward/back/up/down direction;
light plus shield drops instead. An aerial drop is immediately an airborne
item and may hit a legal opponent. During grounded forward/back roll action
ticks 0–4, fresh attack performs glide toss without replacing roll velocity;
the first-late input leaves both roll and held item intact. Fresh attack during
jump squat performs jump-cancel throw, restores the grounded pose, preserves
dash velocity, and transfers momentum to the item. Once takeoff occurs, the
same input is an ordinary aerial throw and cannot cancel the jump.

Typed events 14–18 are `ITEM_PICKUP`, `ITEM_DROP`, `ITEM_THROW`, `ITEM_HIT`,
and `ITEM_RESET`. State schema 26 serializes the complete item state, and
structured observation schema 3 exposes it. RL schema 5 uses compact
observation schema 4 with eight item values at indices 48–55 (position,
velocity, packed state/ownership/direction/hit mask, and three timers). Browser
view schema 22 appends the item and its exact collision extents after the
existing event journal, for 290 values without changing any earlier offset.

`tests/sim/test_m4_item.c` supplies 44 focused invariants, all four directional
throws, positive/negative bat drop, glide toss, and jump-cancel-throw routes,
save/load future equality, encoded replay verification, RL visibility, and
despawn/reset. Browser startup repeats both timing/spacing outcomes for the
three registry techniques before readiness, and the live adapter test performs
an ordinary pickup and throw.

## Pulse Bolt projectile contract

The original Pulse Bolt is one fixed-capacity canonical projectile slot.
Default content keeps it disabled to isolate existing fixtures; the focused
projectile fixture and live browser lab enable the same authored data. Input
schema 4 adds a separate special-button edge. The lowest legal controller slot
wins simultaneous requests, and any request while the slot is occupied is
ignored without creating another entity or event.

A legal grounded or airborne request enters `PROJECTILE_FIRE_GROUND` or
`PROJECTILE_FIRE_AIR`, preserves ordinary aerial drift/gravity where relevant,
and recovers after eight ticks. The bolt spawns 4/5 unit in front of the
fighter, has 1/5-unit half extents, travels straight at 3/5 unit per tick, deals
6%, uses three hitlag ticks, and expires after 120 ticks or on a blast boundary.
The transient `SPAWNING` phase makes the bolt ineligible for collision on its
fire tick; it is exposed as `ACTIVE` after that tick and may hit at most one
legal opponent before the slot clears.

An ordinary shield contact applies the normal projectile shield damage,
pushback, hitlag, stun or break path, then clears the bolt. A shield activated
within the authored two-frame projectile window instead reverses horizontal
velocity, transfers ownership to the defender, sets the visible powershield
result, applies no percent or shield damage, and leaves the bolt active so it
can return through the normal hit path. Typed events 19–21 are
`PROJECTILE_FIRE`, `PROJECTILE_HIT`, and `PROJECTILE_REFLECT`.

`tests/sim/test_m4_projectile.c` supplies 38 focused invariants covering
content validation/hash, simultaneous arbitration, grounded hit, ordinary
block, exact reflect timing and returned hit, short-hop fire and generic
landing, 694-byte save/load future equality, replay verification, and RL
visibility. Strict verifier and browser startup oracles repeat the original
short-hop-laser route. Browser view schema 24 appends 12 projectile values at
indices 290–301 without moving existing offsets, draws the cyan bolt and its
owner, and exposes a live state card. Browser controls are `E` for Player 1,
`;`/Numpad 3 for Player 2, and top face on a Standard Gamepad.

## Prism Burst reflector contract

The original Prism Burst is one immutable reflector definition, not a spawned
entity. Default content keeps it disabled to preserve existing fixture
isolation; the focused reflector fixture and live browser lab enable the same
authored data. Down plus a fresh special edge selects
`REFLECTOR_GROUND` or `REFLECTOR_AIR` from the legal ordinary movement states.
Neutral special remains the Pulse Bolt route. Invalid down-special input is
consumed, and holding special cannot retrigger either action.

Prism Burst has one startup tick, two active ticks, and nine recovery ticks.
Its centered 7/5-by-3/2-unit-half-extent box deals 3%, uses three hitlag ticks,
and sends a physical victim downward through the same damage, hitlag, hitstun,
DI/SDI, event, blast-zone, and stock pipeline as other attacks. The ground and
air actions preserve their respective movement rules; landing the airborne
action uses ordinary generic landing and clears its one-hit mask.

The active box also participates in projectile collision. A Pulse Bolt
overlapping it reverses horizontal velocity, transfers owner to the reflector
user, remains active, emits `PROJECTILE_REFLECT` with the reflector action as
detail, and does not set powershield. This is distinct from the existing
two-frame shield reflection, which retains its powershield result.

`tests/sim/test_m4_reflector.c` supplies 32 invariants covering data validation
and hashing, grounded physical hit, same-tick projectile reflection and
returned hit, an ordinary-input offstage downward hit-to-KO route, an
unchallenged recovery control, save/load future equality, replay verification,
and structured/compact RL visibility. Browser startup repeats both recovery
outcomes before readiness; browser view schema 25 retains the 302-value layout
while naming both actions and exporting the Shine-spike result.

## Arc Reservoir charge storage contract

The original Arc Reservoir is one immutable grounded charge definition.
Default content keeps it disabled so earlier fixtures remain isolated; the
focused charge fixture and live browser lab enable the same authored data.
From grounded idle, walk, initial dash, run, or crouch, full up plus a fresh
special edge selects `CHARGE_GROUND`. Invalid, airborne, hitlag, tumble, held,
or disabled requests are consumed without falling through to Pulse Bolt.

Every charging tick adds one canonical charge tick through an inclusive
120-tick clamp. A fresh shield edge enters `CHARGE_STORE_GROUND`. Releasing
shield before the four-tick store animation completes returns to grounded
idle, retains the stored value, and permits a same-tick ordinary attack. Holding
shield through the animation commits to ordinary shield while retaining the
stored value. A later legal up-special resumes at that exact value.

A fresh light attack during charge selects `CHARGE_RELEASE_GROUND`. Its
four-tick startup, three active ticks, and fourteen recovery ticks use one
ordinary combat hitbox and five hitlag ticks. Damage scales deterministically
from 4% to 20% as stored charge scales from zero to 120 ticks. Completion
clears the charge; a physical hit received during charge or store also clears
it before normal reaction processing.

`tests/sim/test_m4_charge.c` supplies 28 focused invariants covering disabled
and invalid data, accumulation and clamping, early store cancel with a
same-tick ordinary attack, the held-shield negative, exact resume, low/full
release damage, interruption loss, over-cap checksum-valid load rejection,
694-byte mid-store save/load future equality, replay verification, and
structured/compact RL visibility. Browser
startup repeats charge, store cancel, resume, and release before readiness;
browser view schema 26 appends one charge-tick value to each player, shifting
the event/item/projectile blocks by two values and producing a 304-value view.

## Vector Ascent recovery contract

The original Vector Ascent is one immutable recovery definition. Default
content keeps it disabled so focused historical fixtures retain their prior
input routing; the recovery fixture and live browser lab enable the same
authored data. While airborne, full up plus a fresh Special edge selects
`VECTOR_ASCENT` from `AIRBORNE`, `DELAYED_AIR_JUMP`, or `FALL_SPECIAL` only
when the once-per-airtime recovery resource is ready. Grounded full-up Special
remains Arc Reservoir, down Special remains Prism Burst, and neutral Special
remains Pulse Bolt. An unavailable aerial up-special is consumed without
falling through to another special.

Entry spends the resource, clears fast fall and tumble, applies the authored
4/5-unit upward velocity, and derives horizontal velocity from stick input up
to 1/4 unit per tick. Horizontal steering continues during the authored
18-tick action while ordinary gravity changes the vertical trajectory;
fast-fall activation is excluded. Completion enters `FALL_SPECIAL`. Being hit
or otherwise interrupted does not refund the independent resource. Landing,
ledge grab, stock loss/respawn, and reset restore it, so rollback and replay
cannot manufacture an extra recovery.

`tests/sim/test_m4_movement.c` supplies nine focused recovery invariants:
default and invalid data, isolated content hashing, ordinary jump-to-recovery
entry, authored velocity and consumption, structured and compact observation,
694-byte mid-action save/load with equal future hashes, blocked second use,
landing restoration, and second-airtime reuse. Browser startup repeats the
ordinary input entry and exposes `vector_ascent_probe`; live view schema 33
appends one READY/SPENT value per player. Gimp and Stage spike are emergent
compositions of this independently checked recovery with existing aerial or
Prism Burst interruption, solid-surface bounce/tech, and stock/KO mechanics;
they intentionally do not add technique-only state or duplicate harnesses.

## Moonwalk contract

The original fighter authors `moonwalk_setup_ticks=2`. While in
`INITIAL_DASH`, reduced horizontal input opposite the retained facing enters
`MOONWALK_SETUP` at action tick 1. Holding that shallow-back input through
action tick 2 and then switching to full back enters `MOONWALK`, retains the
original facing and dash direction, and applies initial-dash speed in the
opposite direction. Releasing the input returns to `GROUND_IDLE` while normal
traction preserves a decaying backward slide.

Full back immediately after the forward dash is the ordinary initial-dash
reversal. Full back after only one shallow setup tick also falls back to that
same dashback. Neutral or invalid input during setup cancels to grounded idle.
The actions remain interruptible by the existing legal grounded routers and
need no new per-player mutable field: action ID and action ticks carry the
timing through save/load, rollback, replay, and hash.

`tests/sim/test_m4_movement.c` supplies 12 focused invariants covering default
and invalid authored timing, isolated content hashing, the exact two setup
ticks, entry/hold/release velocity and facing, both dashback controls, and a
694-byte mid-setup save/load with equal future hashes. Browser startup repeats
all three timing outcomes and exports an independent `moonwalk_probe` before
readiness. Browser controls use Shift plus the opposite horizontal key for two
ticks, then the unmodified opposite key.

## Teeter-cancel contract

The original fighter authors `teeter_snap_distance_q16=0.4` and
`teeter_ticks=30`. Following the researched
[teeter-cancel route](https://www.ssbwiki.com/Teeter_cancel), neutral
horizontal input converts grounded residual momentum that crosses the facing
support edge within the snap distance into explicit `TEETER`. Entry clamps the
fighter center to the exact support bound, preserves grounding/support/facing,
and clears horizontal velocity and dash direction. Held outward input does not
qualify and runs off; a release that stops before crossing does not qualify.

Neutral `TEETER` persists for action ticks 0–29 and expires to `GROUND_IDLE`.
The ordinary standing input routers remain available on the next tick, so
Attack begins the standing light attack and full opposite input begins a fresh
opposite `INITIAL_DASH` without run-brake delay. The same routing also permits
jump, shield, crouch, special, grab, and reduced-direction walk without a
technique-only input or mutable history field.

`tests/sim/test_m4_movement.c` covers authored-data validation and hashing,
exact entry state, duration, attack and reverse-dash cancels, held-outward and
early-release negatives, and a 694-byte mid-teeter save/load with equal future
hashes. Browser startup repeats both cancels and both negatives and exports an
independent `teeter_cancel_probe` before readiness.

## Stage-humping crouch-step contract

The original fighter authors `crouch_step_speed_q16=0.1` and
`crouch_step_ticks=1`. Following the repeated crouch-step basis of researched
[Stage humping](https://www.ssbwiki.com/Stage_humping), a fresh diagonal-down
edge from `GROUND_IDLE` or `CROUCH` enters explicit `CROUCH_STEP`, faces the
chosen direction, and advances exactly the authored distance. The following
action tick clears horizontal velocity and enters ordinary `CROUCH`.

The existing canonical fresh-down history supplies the release gate. Holding
the diagonal after entry therefore cannot manufacture repeated steps;
releasing and pressing it again can. Neutral down retains stationary crouch,
horizontal input alone retains ordinary dash, and the earlier pass-through
support router retains platform drop. No new mutable state or technique-only
input is introduced.

`tests/sim/test_m4_movement.c` covers authored-data validation and hashing,
exact positive and negative displacement, eight release/reset repetitions,
held-diagonal non-repetition, neutral-down and horizontal-only controls, and a
694-byte mid-step save/load with equal future hashes. Browser startup repeats
the positive route and all controls and exports an independent
`stage_humping_probe` before readiness.

## Taunt-cancel contract

Input schema 5 assigns bit 4 to a dedicated Taunt button. The original fighter
authors `taunt_ticks=90`. A fresh grounded Taunt edge from ordinary idle, walk,
initial dash, run, crouch, run turnaround/brake, or teeter enters explicit
`TAUNT`; inherited horizontal velocity decelerates under ordinary traction,
while attack, jump, shield, dodge, grab, movement, and held-Taunt retriggering
remain locked until the exact recovery boundary.

Following the researched
[taunt-cancel route](https://www.ssbwiki.com/Taunt_canceling), releasing the
horizontal input and pressing Taunt while retained dash momentum crosses the
facing support edge lets the existing support-edge transition take priority.
It clamps the fighter at the support bound and replaces `TAUNT` with `TEETER`
well before tick 90. Starting farther from the edge instead completes all 90
ticks and returns to `GROUND_IDLE`.

`tests/sim/test_m4_movement.c` covers authored-data validation and hashing,
dash-momentum entry, exact recovery and input lock, held-button non-repetition,
edge cancellation, and a 694-byte mid-taunt save/load with equal future
hashes. Browser startup repeats the full-duration and cancel routes and exports
an independent `taunt_cancel_probe` before readiness.

## Scar-Jump and normal-wall-jump contract

The original fighter authors `wall_jump_speed_x_q16=0.3`,
`wall_jump_speed_y_q16=-0.5`, `wall_jump_ticks=24`,
`wall_jump_invulnerability_ticks=4`, and `wall_jump_enabled=1`. A fresh full
direction away from an exact solid-wall contact enters explicit `WALL_JUMP`,
launches away and upward, and preserves the fighter's remaining air jump.
Ordinary airborne gravity continues during the action. Attack can cancel the
action into either authored aerial, while a fresh jump spends the saved air
jump; otherwise movement and special routing stay locked through the exact
24-tick boundary.

The production route starts with the ordinary right-ledge jump, travels inward
to the raised block, and applies the fresh away direction only at wall contact.
The first four action ticks are invulnerable. Holding away before reaching the
wall changes the trajectory and never creates a wall jump, providing the
negative timing control. This is the original-stage equivalent of the
[Scar Jump](https://www.ssbwiki.com/Scar_Jump): it uses the normal wall-jump
primitive while keeping the midair jump for a deeper recovery or edgeguard.

`tests/sim/test_m4_movement.c` covers authored-data validation and hashing,
production ledge/block geometry, exact launch, preserved air jump, the four-
tick invulnerability and 24-tick action windows, aerial and saved-jump cancels,
the early-away negative, and a 694-byte mid-action save/load with equal future
hashes. Browser startup repeats the positive and negative routes and exports an
independent `scar_jump_probe` before readiness.

## Team-Wobble handoff contract

The optional four-player browser lab uses production `PF_SIM_MODE_TEAMS` with
P1/P3 allied against P2/P4. The stage uses 0.4-unit spawn spacing and moves the
pass-through platform outside the handoff lane; it changes no fighter or throw
data. P1 and P3 remain ordinary player-controlled fighters, captured P2 emits
alternating legal mash edges, and P4 stays neutral.

Once one ally holds P2, that holder starts the existing low down throw while
the opposite ally makes a fresh light-plus-shield grab request. The throw
clears the first reciprocal link, applies its ordinary hitlag and damage, and
the waiting active grabbox creates the next reciprocal link after release.
Repeating from the opposite side produces two alternating typed throw and grab
events. Starting the waiting grab during the initial capture spends its active
window before the throw releases, so P2 escapes instead. This is the original
fixture for the researched [Team wobble](https://www.ssbwiki.com/Team_wobble)
pattern and adds no technique-only combat branch.

`tests/sim/test_m4_combat.c` covers both handoffs, exact event attribution,
links, damage, legal victim mash, same-team rejection, and the early-grab
control. Browser startup independently repeats the positive and negative route
and exports `team_wobble_probe`; the live Team Wobble Lab maps the second
physical controller to simulation slot 2 rather than the scripted victim.

## Canonical state and inspection

Browser view schema 33 expands the presentation-only view from 392 to 396
values by appending one recovery-availability value per possible player at
indices 392–395. Player blocks remain 44 values each at base 25; event count
remains at 201, the 16 ten-value event entries start at 202, the 18-value item
block starts at 362, and the 12-value projectile block starts at 380.

State schema 36 / save format 35 expands the stream to 694 bytes (140-byte
header plus 554-byte payload), changes the active magic to `PFSAVE35`, appends
one canonical recovery-availability byte per player, and makes the
`VECTOR_ASCENT` action, legal source actions, authored tick range, consumption,
blocked reuse, special-fall completion, and restoration paths fail closed.
Inspection schema 32 and structured observation schema 6 expose the byte.
RL schema 8 / compact observation schema 7 packs it into player flag bit 18
without changing the 66-value vector. Content schema 37 adds one recovery
definition under recovery schema 1; fighter schema 32 and input schema 5 remain
unchanged.

Browser view schema 32 previously expanded the presentation-only view from 304
to 392 values so all four inspection records could be rendered, with no
canonical state or API schema change.

It follows state schema 35 / save format 34, which retained the 690-byte stream
(140-byte header plus 550-byte payload), used `PFSAVE34`, and made the Wall-Jump
action ID, airborne state, authored tick range, brief invulnerability,
preserved air jump, wall-contact entry, and legal jump/aerial cancels fail
closed. Inspection schema 31 and browser view schema 31 versioned the action
interpretation without changing the former 304-value browser layout. Content
schema 36/fighter schema 32 added and hashed the authored speeds, duration,
invulnerability, and enable flag. Input schema 5, structured observation
schema 5, RL schema 7, compact observation schema 6, and its 66 values remained
unchanged.

It follows state schema 34 / save format 33, which retained the 690-byte stream,
used `PFSAVE33`, and made the Taunt action ID, grounding, authored tick range,
locked recovery, inherited momentum, held-input non-repetition, and
support-edge cancellation fail closed. Inspection/browser view schema 30 and
content schema 35/fighter schema 31 versioned and hashed that interpretation;
input schema 5 added the dedicated bit-4 Taunt control.

It follows state schema 33 / save format 32, which retained the 690-byte
stream, used `PFSAVE32`, and made the crouch-step action ID, grounding,
authored tick range, fresh diagonal-down entry, and ordinary-crouch transition
fail closed. Inspection/browser view schema 29 and content schema 34/fighter
schema 30 versioned and hashed that interpretation.

It follows state schema 32 / save format 31, which retained the 690-byte
stream, used `PFSAVE31`, and made the Teeter action ID, grounding, zero
horizontal velocity, authored tick range, edge entry, and legal cancel
semantics fail closed. Inspection/browser view schema 28 and content schema
33/fighter schema 29 versioned and hashed that interpretation.

It follows state schema 31 / save format 30, which retained the 690-byte
stream, used `PFSAVE30`, and made both Moonwalk action IDs,
setup/activation timing, retained facing/dash direction, reverse velocity, and
mistimed dashback semantics fail closed. Inspection/browser view schema 27 and
content schema 32/fighter schema 28 versioned and hashed that interpretation.

It follows state schema 30 / save format 29, which expanded the stream to 690
bytes, used `PFSAVE29`, appended one canonical charge-tick value per player,
and made all three charge action IDs, storage, resume, release, scaling,
completion, and interruption semantics fail closed. Inspection schema 26 and
browser view schema 26 exposed the charge value and action interpretation.
Structured observation schema 5 exposes charge per player. RL schema 7 and
compact observation schema 6 append four charge values at indices 62–65 for
66 total values without moving earlier compact indices. Content schema 31
adds one charge definition under charge schema 1.

It follows state schema 29 / save format 28, which retained the 682-byte
stream, used `PFSAVE28`, and made the two reflector action IDs, hitlag resume,
landing, downward-launch, and active-box projectile-reflection semantics fail
closed without adding mutable fields. Inspection schema 25 and browser view
schema 25 exposed the new action interpretation without changing their
layouts. It follows state schema 28 / save format 27, which added the complete
fixed projectile slot and made fire, collision, block, powershield reflection,
hit, ownership, action, and typed-event semantics fail closed. Structured
observation schema 4 exposed the same slot. RL schema 6 and compact observation
schema 5 appended six projectile values at indices 56–61 without moving earlier
indices. State
schema 28 / save format 27 follows state schema 27 / save format 26, which
retains the 662-byte stream and makes the full-up jump-squat attack cancel,
grounded standing-strong selection, inherited momentum, and
neutral/shallow/late exclusions fail closed without adding mutable fields. It
follows state schema 26 / save format 25, which expanded the
stream to 662 bytes and added the fixed item entity, timers,
ownership/attribution, hit mask, throw direction,
`ITEM_THROW`/`ITEM_DASH_THROW` action semantics, and typed item events. It
follows state schema 25 / save format 24, which retained the
635-byte stream and made
knockback-based delayed-air-jump armor, zero-launch hit events, preserved
trajectory/action timing, and `DELAYED_AIR_JUMP` hitlag resume fail closed
without adding mutable fields. It follows state schema 24 / save format 23,
which made the `DELAYED_AIR_JUMP` action ID, authored half-open aerial-cancel window,
remaining-upward-velocity cancellation, late full-arc behavior, and
simultaneous jump-plus-attack non-consumption fail closed without adding
mutable fields. It follows state schema 23 / save format 22, which made the
`RESET_BOUND` and `FORCED_GETUP` action IDs, hitlag resume, authored action
schedule, reaction eligibility, and grounded-versus-airborne expiry semantics
fail closed without adding mutable fields. It follows state schema 22 / save
format 21, which made the `JAB_FINAL` action ID, hitlag resume, authored action
schedule, and jab choice window semantics fail closed without adding mutable
fields. That format follows state schema 21 / save format 20, which made the
`DASH_ATTACK` action ID, hitlag
resume, authored action schedule, run entry, and boost-grab cancel semantics
fail closed without adding mutable fields. That format follows state schema 20
/ save format 19, which made the four throw action IDs,
thrower hitlag resume, startup-link, atomic release, and post-release recovery
semantics fail closed without adding mutable fields. That format follows state
schema 19 / save format 18, which added one escape timer, one
target slot, and one owner slot per player; load requires every live link to be
in range, reciprocal, and action-compatible. That format follows state schema
18 / save format 17, which added one remaining
ledge-regrab-lockout timer per player, and state schema 17 / save format 16,
which added one remaining ledge-invulnerability timer per player. The
invulnerability timer is
refreshed by a legal catch, survives ledge options, and participates in the
same production hit-ownership rejection as other invulnerability. It follows
state schema 16 / save format 15, which retained the 603-byte stream and added the
canonical `SHIELD_BREAK_DOWN`, `SHIELD_BREAK_STAND`, and
`SHIELD_BREAK_STUN` action semantics without adding mutable fields: the
existing action timer is elapsed time for down/stand and remaining time for
stun. It follows state schema 15 / save format 14, which defined the ABI-4
typed per-tick event journal while keeping only its authoritative monotonic
sequence in canonical state, and state schema 14 / save
format 13, which added the stock rules, per-player stocks, respawn timers,
sudden-death state, and `RESPAWN_WAIT`/`ELIMINATED` action semantics, and state
schema 13 / save format 12, which added `STRONG_AERIAL_ATTACK`,
`STRONG_AERIAL_LANDING`, and
`STRONG_L_CANCEL_LANDING` semantics without changing the byte layout, and
state schema 12 / save format 11, which
added `ROLL_FORWARD`, `ROLL_BACKWARD`, and `SPOT_DODGE` semantics plus one
canonical fresh-down history byte per player, and state schema 11 / save format
10, which added `AERIAL_ATTACK`,
`AERIAL_LANDING`, and `L_CANCEL_LANDING` semantics plus one canonical
trigger-age byte per player, and state schema 10's `AIR_DODGE`, `FALL_SPECIAL`,
and `SPECIAL_LANDING` semantics and the state-schema-9 `WALL_TECH`,
`WALL_TECH_JUMP`, `CEILING_TECH`, `WALL_BOUNCE`, and `CEILING_BOUNCE` action
semantics plus the solid-top support ID. Input schema 3 still supplies the
separate light- and strong-attack buttons.

Content schema 27 / fighter schema 27 adds and hashes the inclusive
`double_jump_armor_max_hitstun_ticks` threshold. Zero disables armor; the
value is range-checked, and an authored armor threshold requires a nonzero
double-jump-cancel window. It follows schema 26's double-jump-cancel window;
zero disables the delayed action and values above 120 are rejected. Schema 26
follows schema 25's reset maximum damage, maximum hitstun, bound duration and
speed, and forced-getup duration. It follows
schema 24's inclusive first-jab combo-input window plus the final jab's hitbox
geometry, damage, signed base launch and per-percent growth, startup, active,
recovery, and hitlag. That schema follows schema 23's dash-attack speed, hitbox
geometry, damage, signed base
launch and per-percent growth, startup, active, recovery, hitlag, and the
boost-grab cancel window, and schema 22's four throws' damage, signed base
launch and per-percent growth, release
tick, recovery, and hitlag, and schema 21's grabbox geometry, held offset,
damage-scaled escape
duration, startup/active/recovery timing, base/maximum escape timing,
fresh-input mash reduction, and release timing, schema 20's 29-tick
ledge-regrab lockout, and schema 19's reduced-down
shield platform-drop threshold,
schema 18's validated V-cancel velocity
scale and input window and schema 17's validated drop-cancel
snap distance and nine-tick default platform pass timer and schema 16's
validated three-tick forward-smash input window
and schema 15's validated
37-tick default ledge invulnerability duration and schema 14's independently validated
shield-break launch speed, base/minimum stun, down/stand durations, and mash
reduction, and schema 13's strong-aerial landing-lag duration and
schema 12's roll
speeds/durations, shared roll movement and invulnerability windows, and
spot-dodge duration/invulnerability, plus schema 11's light-aerial hitbox,
phase, landing-lag, and L-cancel data. Stage schema 2 retains the solid block
bounds.

Loading validates every new timer, flag, direction, action relationship,
inactive slot, and pending-launch bound before replacing live state. Saving
during hitlag and continuing after load must produce the same per-tick hashes.

Inspection schema 22 identifies the delayed-air-jump armor and hitlag-resume
contract while retaining schema 21's delayed-air-jump state/content contract,
schema 20's jab-reset contract, schema 19's jab-sequence
contract, schema 18's dash-attack contract,
schema 17's throw contract, and
schema 16's grabbox bounds/active
state, escape ticks, and reciprocal
target/owner slots. It retains schema 15's exact
remaining ledge-invulnerability and regrab-lockout timers and schema 14's
percent, hitlag, hitstun, tumble, tech window and
lockout, trigger-held state, SDI count/direction, tech direction, shield
health/stun/powershield, derived ledge/tech/air-dodge invulnerability, active hitbox
bounds, last-hit metadata, solid-block geometry, trigger age, and derived
L-cancel eligibility, plus stock rules, remaining stocks, respawn timers,
sudden death, and result. Browser view schema 21 carries the
double-jump-cancel-counter readiness probe while retaining schema 20's
`DELAYED AIR JUMP` label and double-jump-cancel readiness probe, schema 19's `RESET
BOUND`, `FORCED GETUP`, and jab-reset readiness probe and schema 18's
`JAB FINAL` and jab-cancel probe, `DASH ATTACK`, the boost-grab readiness probe,
the throw action/event identities, and the grab fields. It
retains its derived invulnerability marker rather than exporting either exact
ledge timer, and
carries the prior combat fields plus the canonical action timer, floor action semantics,
the live shield bubble, a visibly rotating tumble
presentation, a prone missed-tech pose, recovery invulnerability, and the
floor-attack hitbox, the strong-aerial states, the landing-result banner/ring
and countdown, stock HUD, countdown/result overlays, the most recent per-tick
event records, and renders the block. It labels every shield-break phase,
renders the down phase prone, and gives vulnerable stun an orbiting-star
`MASH · Nf` countdown.

## Deterministic event journal

Every successful ABI-4 tick returns a zero-initialized fixed-capacity journal
of up to 16 typed events. Current combat and match producers emit:

- hit, shield block, powershield, shield break, grab, grab escape, and throw;
- KO and respawn, including remaining-stock, elimination, and sudden-death
  flags;
- sudden-death setup, match result, forfeit, and time limit.

Each fixed 32-byte record contains the processed input tick, match-monotonic
sequence, source/target slots, Q16.16 value and velocity fields, flags, and a
type-specific detail. Player `255` means system/no player. Target resolution
and event order follow stable slot order, with match resolution last.

| Event | Value / velocity | Detail |
|---|---|---|
| Hit | Attack damage / pending launch | Attacker action |
| Shield block, powershield, hit-caused shield break | Applied shield damage / physical pushback | Attacker action |
| Hold-depletion shield break | Actual depleted shield health / upward launch | Zero |
| Grab | Victim percent / zero | Grab action |
| Grab escape | Victim percent / zero | Zero |
| Throw | Applied damage / authored launch before DI | Throw action |
| KO | Pre-reset percent / blast-crossing velocity | Stocks remaining |
| Respawn | Respawn percent / spawn velocity | Invulnerability ticks |
| Sudden death | `300%` / zero | Player count |
| Match result | Zero | Winner mask |
| Forfeit | Zero | Zero |
| Time limit | Zero | Zero |

The tumble flag annotates a hit; eliminated/last-stock annotate a KO; and
sudden-death annotates setup, respawn, KO, and result events where applicable.

The array is caller-owned output for one tick and is not serialized as rolling
history. The canonical save stores the sequence authority. A mid-match save,
load, and identical continuation must return byte-identical journals, which
prevents duplicated or renumbered rollback effects without making state size
depend on match length. Capacity 16 exceeds the statically proven current
maximum of 13; overflow or sequence exhaustion is a deterministic fault.

## Verification

`tests/sim/test_m4_combat.c` and `tools/verify_m4_combat.sh` cover 492 focused
mechanics invariants plus 50 journal invariants, including:

- light, strong, and aerial attack schedules, facing, whiff, damage, ownership,
  freeze,
  launch, hitstun, one-hit masks, simultaneous trades, and the default strong
  attack's direct tumble-to-knockdown route;
- aerial hitlag freezing both airborne fighters, resuming the attacker in its
  aerial, one-hit-per-target behavior, and a focused per-tick-hash replay that
  records short hop, aerial, fast fall, eligible trigger, and L-cancel landing;
- frame-perfect drop-cancel hitlag expiring the platform timer, exact
  same-platform snap and 12-tick aerial landing lag, a one-tick-late connecting
  attack and a frame-perfect whiff both falling through, invalid data, and
  mid-route save/load future-hash equality;
- exact V-cancel trigger ages 0–2 and age-3 rejection, 95% scaling of both
  launch components with unchanged hitstun/tumble, grounded and aerial-attack
  exclusions, repeated-trigger lockout, invalid data, and byte-identical
  mid-route save/load event/hash continuation;
- exact grab startup/active/recovery phases, shield bypass, spot-dodge and
  same-team rejection, lower-port collision priority, reciprocal capture
  links, percent scaling/cap, natural and fresh-input mash escape, jump-squat
  cancel with retained dash momentum, direct-dash and airborne negatives,
  typed grab/escape events, invalid data, and mid-hold save/load event/hash
  continuation;
- all four directional throw selections and exact startup/release/hitlag/
  recovery schedules, horizontal diagonal-tie and vertical-dominance boundaries,
  neutral/reduced-input rejection, signed percent-scaled launch,
  reciprocal link clearing, typed throw events, a three-throw/two-regrab chain,
  a 96% outward-DI regrab whiff, invalid throw data, and mid-chain save/load
  event/hash continuation;
- ordinary dash grab at run momentum; production dash-attack entry, inactive
  startup, exact authored active hit and typed identity; boost-grab cancellation
  on the three legal frames with faster retained momentum and expanded-range
  capture; same-frame ordinary selection and late-cancel rejection; invalid
  speed/window data; and mid-dash-attack save/load event/hash continuation;
- first-jab shield cancellation on hit and whiff at the exact inclusive
  boundaries, early-held and first-late rejection, independent final-jab
  damage/typed identity, invalid timing/hit data, and mid-window save/load
  event/hash continuation;
- the responder's short jab whiffing at the safe 1.95-unit band before a
  longer strong counter connects during recovery, the 1.7-unit close punish,
  2.25-unit double whiff, safe-tip shield block, and mid-counter save/load with
  32 future hashes;
- reduced-stick walking from the default 16-unit neutral separation into that
  safe band, the resulting whiff conversion, and the overextended approach
  being intercepted by the same jab-first responder;
- a short-hop back-aerial cross-up finishing behind held shield, an immediate
  wrong-side whiff, a forward-facing front-side block control, and mid-aerial
  save/load with 48 equal future hashes;
- the combined mindgame gate requiring the jab-read spacing conversion,
  shield-read cross-up, strong-into-shield wrong read, and immediate aerial
  whiff to pass as one ordinary-input tactic;
- a 12% grounded launcher into an 8% airborne aerial before landing,
  mid-launch save/load with equal future hashes, and an active follow-up whiff
  against directional influence plus a fresh directional air dodge;
- a 126% jab setup remaining locked into a 138% strong-attack KO, typed
  attacker attribution, setup-to-KO save/load hashes, an 18% low-percent
  survival control, and a high-percent outward-DI active whiff;
- below-platform light-aerial initiation into an 8% platform-opponent hit, a
  too-early active-hitbox whiff, ordinary held-shield damage with a typed block
  event, and mid-aerial save/load with 32 equal future hashes;
- strong-aerial entry, active-frame damage/hitlag/event ownership, and exact
  post-hitlag resume into the airborne strong action;
- first-component SDI, held-direction rejection, diagonal second-component
  SDI, ASDI/DI launch application, approximate speed preservation, and
  deterministic direction;
- missed tech, 26-tick in-place tech, 40-tick directional tech roll, 20-tick
  input window/lockout behavior, exact 20-tick hit rejection, vulnerability
  restoration, and held-trigger edge behavior;
- exact 26-tick missed-tech animation, persistent/automatic down-wait,
  up/shield neutral getup, bidirectional getup roll, all three recovery
  durations and invulnerability cutoffs, front/back floor-attack hits with
  negative timing checks, and mid-roll save/load continuation;
- airborne following into observed tech-in-place and right-tech-roll outcomes,
  jabs during both vulnerable recovery tails, a same-action-tick static jab
  that misses the roll, and mid-roll save/load future-hash equality through
  the reacting punish;
- air-dodge invulnerability rejecting an overlapping production jab inside
  the window and accepting the same hit on the exact expired boundary;
- spot-dodge hit acceptance immediately before its invulnerability, rejection
  at the first invulnerable tick, acceptance at the first
  recovery-vulnerable tick, and forward-roll hit rejection at its first
  invulnerable tick;
- production strong-attack routes into wall tech, wall-tech jump, ceiling
  tech, missed wall bounce, and missed ceiling bounce; exact stall/release,
  facing, velocity, reaction-state, and invulnerability checks; and invalid
  solid-geometry rejection;
- rejection of invalid reaction content;
- run shield stop, the initial-dash shield restriction, hold depletion,
  minimum hold, bounded action timer, release lag, regeneration, and jump
  cancel;
- one-tick dashing-shield tap/release versus held shield stop with exact
  position, velocity, action, health, and release-duration boundaries; idle
  no-travel; and mid-route save/load with equal future hashes;
- ordinary block damage/stun/hitlag/pushback, four-tick physical powershield,
  zero powershield damage, larger powershield pushback, and result-flag
  clearing;
- physical powershield opportunity preservation, frame-1 rejection, frame-2
  attack cancel, ordinary-shield negative behavior, content validation, and a
  focused encode/verify replay that performs the cancel;
- deterministic hit/depletion shield-break events, upward flight and gravity,
  forced landing, exact down/stand order, percent-scaled vulnerable stun,
  fresh-input mash reduction with a held-input negative case, early-phase hit
  rejection, stun interruption, 30-HP reset, invalid content rejection, and
  mid-stun save/load with equal future hashes;
- mid-hitlag and mid-shield-hitlag save/load with equal future hashes; and
- a 20,000-tick four-player team trace with a canonical hash after every tick.

`tests/sim/test_m4_match.c` and `tools/verify_m4_match.sh` add 24 match
invariants plus 44 journal invariants: rule defaults and invalid bounds, stock
loss, exact respawn and
invulnerability boundaries, hit rejection/acceptance around invulnerability,
mid-respawn save/load continuation, final-stock result, 300% simultaneous-KO
sudden death, deterministic repeated-tie resolution, and 2v2 team winner
masks.

The 180-tick replay corpus includes vertical stick and trigger inputs and
requires observed grounded-roll, spot-dodge, SDI, tech-window, air-dodge, and
special-landing state before
encoding. Native
and WebAssembly runs must agree on all 181 state hashes, the 31,386-byte
replay, its final digest, and the complete typed event stream digest under the
`PFEVT001` domain.

The browser startup refuses readiness unless independent movement,
drop-cancel, V-cancel, bat-drop, glide-toss, jump-cancel-throw, jump-cancel
attack, planking, short-hop laser, Shine spike, charge storage, Vector Ascent,
jump-canceled-grab, boost-grab, jab-cancel,
chain-grab,
ground-dodge, air-dodge,
attack, reaction, shield, shield-break, tumble,
floor-recovery, tech-chase, and surface-tech probes pass. The Vector Ascent
probe performs an ordinary jump, enters the recovery with full-up fresh
Special, and requires the authored horizontal/upward velocities plus visible
resource consumption. The tech-chase probe
strong-launches the target, follows its airborne path, reacts separately to
tech in place and a right tech roll, jabs after invulnerability, and requires a
same-timed non-following jab to miss the roll. The
V-cancel probe moves the default fighters together, compares ordinary launch
with a collision-frame trigger, requires exact 95% two-axis scaling and
unchanged hitstun, then proves an active aerial and a repeated trigger inside
the 40-tick lockout both receive ordinary launch. The
surface probe moves the ordinary default fighters near the raised block,
strong-launches a tumbling target, opens the real trigger window during
flight, holds up, and requires `WALL_TECH_JUMP` with cleared reaction state.
The floor probe
observes exact knockdown-to-down-wait timing, all three input outcomes,
recovery invulnerability, and both floor-attack active phases. The shield probe
observes a normal physical block, a four-frame powershield, the frame-1
shield-drop delay, and a frame-2 powershield-canceled attack through the
production collision path.
The independent shield-break probe holds the ordinary trigger through
depletion, requires the system-authored break event and upward launch, observes
down/stand/stun in order, proves fresh-versus-held mash timing, and reaches the
30-HP recovery.
The air-dodge probe independently reaches directional `AIR DODGE`, its
invulnerability window, `FALL SPECIAL`, and a first-airborne-frame diagonal
`SPECIAL LANDING` with continued horizontal slide.
The grounded-dodge probe independently reaches relative-facing forward and
backward roll plus spot dodge and observes each authored invulnerability
window.
The aerial probe independently compares generic auto-cancel landing, 12-tick
normal aerial landing, and six-tick L-cancel landing, then proves eligible
trigger ages 0–6 and the exact ineligible age-7 boundary. It then exercises the
strong aerial and requires exactly 30 normal or 15 L-cancel landing ticks.
The match probe uses ordinary horizontal input to cross a blast boundary,
requires exactly one stock loss and the full 60-tick inactive wait, then
requires an active respawn with the full 120-tick invulnerability timer.
