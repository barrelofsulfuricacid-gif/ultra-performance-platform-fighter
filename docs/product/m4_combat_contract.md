# M4 combat and hit-reaction checkpoint contract

## Scope

This checkpoint extends the first production-path M4.2 ground attacks with
deterministic hit reaction and the first dense-shield primitive: trajectory
DI, SDI, ASDI, tumble, missed-tech knockdown, tech in place, directional
ground tech, shield stop, shield damage/stun/pushback, shield release and
regeneration, grounded shield break lockout, physical powershielding, and
frame-2 powershield canceling into either current production ground attack.
These primitives use the same normalized input, simulation, save/load, replay,
RL, and browser paths.

This is still an incremental checkpoint. It does not claim the remaining
attacks, analog light shields, shield tilt/size/pokes, shield SDI, rolls,
spot dodge, platform shield drop, grabs, projectile powershields, complete
shield-break launch/stun, wall/ceiling techs, get-up
choices, tech invulnerability, stocks, match completion, or completion of the
61-row non-character-specific advanced-technique gate.

## Attack, collision, and ownership

The light or strong ground attack is entered by a rising edge on its separate
input-schema-3 button while the fighter is grounded and outside a locked
action. If both edges occur on one tick, strong attack takes priority. There is
no universal input buffer. The default light jab defines two startup ticks, two
active ticks, eight recovery ticks, and four hitlag ticks. The default strong
attack defines five startup ticks, three active ticks, 18 recovery ticks, and
six hitlag ticks.

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
- no open window enters `KNOCKDOWN` for 30 ticks.

The placeholder now uses Melee's universal 26-tick tech-in-place and 40-tick
tech-roll durations; both reject hits for their first 20 ticks and expose that
derived invulnerability through inspection. `KNOCKDOWN` remains vulnerable.
These actions currently return directly to ground idle. Missed-tech get-up
choices, attack interruption, and wall/ceiling techs remain explicit follow-up
work and are not implied by these state names. The timing contract follows the
[Melee tech frame-data summary](https://www.reddit.com/r/smashbros/comments/1svuas/when_is_it_possible_to_hit_an_opponent_who_missed/)
alongside the 20-frame input window and 40-frame lockout described by
[SmashWiki](https://www.ssbwiki.com/Tech).

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
- reset to 30 HP after the current shield-break lockout.

Shield release regenerates health because the blocking volume is no longer
active. Holding shield cannot reopen a tech window without a new trigger edge,
so the existing tech-window/lockout contract remains intact.

## Blocking, shield stun, and powershield

Each current physical ground attack intersects the grounded fighter body box as
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
The powershield result flag remains inspectable through hitlag and shield stun.
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
enters `SHIELD_BREAK`. The state is grounded, locked, and deterministic for
180 ticks, ignores further hitboxes during this placeholder lockout, then
restores 30 shield HP. This is only the canonical state and serialization
foundation. It does not yet claim Melee's upward shield-break launch,
landing/knockdown sequence, damage-dependent stun, mash reduction, vulnerability,
or hit interruption.

## Canonical state and inspection

State schema 7 / save format 6 adds the canonical `STRONG_ATTACK` action
semantic while retaining the 569-byte stream: a 140-byte header plus a
429-byte payload. The active magic is `PFSAVE06`. Input schema 3 adds the
separate strong-attack button.

Content schema 7 adds independent strong-attack geometry, damage, knockback,
startup, active, recovery, and hitlag data. No new canonical state field is
required: the new action ID and existing action timer fully identify the strong
attack, and the existing powershield flag still distinguishes an eligible
release.

Loading validates every new timer, flag, direction, action relationship,
inactive slot, and pending-launch bound before replacing live state. Saving
during hitlag and continuing after load must produce the same per-tick hashes.

Inspection schema 6 exposes percent, hitlag, hitstun, tumble, tech window and
lockout, trigger-held state, SDI count/direction, tech direction, shield
health/stun/powershield, derived tech invulnerability, active hitbox bounds,
and last-hit metadata. Browser
view schema 5 carries those fields, the new action semantic, the live shield
bubble, and a visibly rotating tumble presentation after hitlag.

## Verification

`tests/sim/test_m4_combat.c` and `tools/verify_m4_combat.sh` cover 57 focused
invariants, including:

- light and strong attack schedules, facing, whiff, damage, ownership, freeze,
  launch, hitstun, one-hit masks, simultaneous trades, and the default strong
  attack's direct tumble-to-knockdown route;
- first-component SDI, held-direction rejection, diagonal second-component
  SDI, ASDI/DI launch application, approximate speed preservation, and
  deterministic direction;
- missed tech, 26-tick in-place tech, 40-tick directional tech roll, 20-tick
  input window/lockout behavior, exact 20-tick hit rejection, vulnerability
  restoration, and held-trigger edge behavior;
- rejection of invalid reaction content;
- run shield stop, the initial-dash shield restriction, hold depletion,
  minimum hold, bounded action timer, release lag, regeneration, and jump
  cancel;
- ordinary block damage/stun/hitlag/pushback, four-tick physical powershield,
  zero powershield damage, larger powershield pushback, and result-flag
  clearing;
- physical powershield opportunity preservation, frame-1 rejection, frame-2
  attack cancel, ordinary-shield negative behavior, content validation, and a
  focused encode/verify replay that performs the cancel;
- deterministic shield break, placeholder re-hit lockout, reset health, and
  invalid shield-data rejection;
- mid-hitlag and mid-shield-hitlag save/load with equal future hashes; and
- a 20,000-tick four-player team trace with a canonical hash after every tick.

The 180-tick replay corpus includes vertical stick and trigger inputs and
requires observed SDI, tech-window, and shield state before encoding. Native
and WebAssembly runs must agree on all 181 state hashes, the 31,261-byte
replay, and its final digest.

The browser startup refuses readiness unless independent movement, attack,
reaction, and shield probes pass. The shield probe observes a normal physical
block, a four-frame powershield, the frame-1 shield-drop delay, and a frame-2
powershield-canceled attack through the production collision path.
