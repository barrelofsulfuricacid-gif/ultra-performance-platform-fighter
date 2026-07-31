# M4 combat and hit-reaction checkpoint contract

## Scope

This checkpoint extends the first production-path M4.2 ground attacks with
deterministic hit reaction and the first dense-shield primitive: trajectory
DI, SDI, ASDI, tumble, missed-tech knockdown/down-wait, tech in place,
directional ground tech, reaction-driven tech chasing, wall tech,
wall-tech jump, ceiling tech,
missed wall/ceiling bounce, neutral getup, getup roll, two-sided floor attack,
shield stop, dashing shield, shield damage/stun/pushback, shield release and
regeneration, complete shield-break launch/down/stand/stun/recovery,
physical powershielding, and
frame-2 powershield canceling into either current production ground attack.
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
attacks, analog light shields, shield tilt/size/pokes, shield SDI,
platform shield drop, grabs, projectile powershields, complete
prone-orientation-specific
getup-roll asymmetry, a moving revival platform, or completion of the 61-row
non-character-specific advanced-technique gate. Configurable stocks, delayed
respawn, respawn invulnerability, elimination, team results, rematch, and
simultaneous-final-stock sudden death are now part of the checkpoint.

## Attack, collision, and ownership

The light or strong ground attack is entered by a rising edge on its separate
input-schema-3 button while the fighter is grounded and outside a locked
action. If both edges occur on one tick, strong attack takes priority. An
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

## Canonical state and inspection

State schema 17 / save format 16 expands the stream to 611 bytes (140-byte
header plus 471-byte payload) and changes the active magic to `PFSAVE16`. It
adds one remaining ledge-invulnerability timer per player. The timer is
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

Content schema 15 / fighter schema 15 adds the validated 37-tick default ledge
invulnerability duration. It follows schema 14's independently validated
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

Inspection schema 14 exposes percent, hitlag, hitstun, tumble, tech window and
lockout, trigger-held state, SDI count/direction, tech direction, shield
health/stun/powershield, derived ledge/tech/air-dodge invulnerability, active hitbox
bounds, last-hit metadata, solid-block geometry, trigger age, and derived
L-cancel eligibility, plus stock rules, remaining stocks, respawn timers,
sudden death, and result. Browser view schema 14
carries those fields plus the canonical action timer, floor action semantics,
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

- hit, shield block, powershield, and shield break;
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

`tests/sim/test_m4_combat.c` and `tools/verify_m4_combat.sh` cover 149 focused
mechanics invariants plus 30 journal invariants, including:

- light, strong, and aerial attack schedules, facing, whiff, damage, ownership,
  freeze,
  launch, hitstun, one-hit masks, simultaneous trades, and the default strong
  attack's direct tumble-to-knockdown route;
- aerial hitlag freezing both airborne fighters, resuming the attacker in its
  aerial, one-hit-per-target behavior, and a focused per-tick-hash replay that
  records short hop, aerial, fast fall, eligible trigger, and L-cancel landing;
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
and WebAssembly runs must agree on all 181 state hashes, the 31,303-byte
replay, its final digest, and the complete typed event stream digest under the
`PFEVT001` domain.

The browser startup refuses readiness unless independent movement,
ground-dodge, air-dodge, attack, reaction, shield, shield-break, tumble,
floor-recovery, tech-chase, and surface-tech probes pass. The tech-chase probe
strong-launches the target, follows its airborne path, reacts separately to
tech in place and a right tech roll, jabs after invulnerability, and requires a
same-timed non-following jab to miss the roll. The
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
