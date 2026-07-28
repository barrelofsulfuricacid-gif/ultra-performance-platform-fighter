# M4 combat and hit-reaction checkpoint contract

## Scope

This checkpoint extends the first production-path M4.2 ground attack with the
first deterministic defensive reaction layer: trajectory DI, SDI, ASDI,
tumble, missed-tech knockdown, tech in place, and directional ground tech.
These primitives use the same normalized input, simulation, save/load, replay,
RL, and browser paths.

This is still an incremental checkpoint. It does not claim the remaining
attacks, shields, wall/ceiling techs, get-up choices, tech invulnerability,
stocks, match completion, or completion of the 61-row non-character-specific
advanced-technique gate.

## Attack, collision, and ownership

The ground attack is entered by a rising edge on the input-schema-2 attack bit
while the fighter is grounded and outside a locked action. There is no
universal input buffer. The default data defines two startup ticks, two active
ticks, eight recovery ticks, and four hitlag ticks.

- Fighter content defines hitbox offset and extents, damage, base launch,
  damage growth, hitstun conversion, and phase durations.
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

Damage is unsigned Q16.16 percent and saturates at 999%. The default attack
adds 6%. Launch uses post-hit damage:

- horizontal launch is facing-signed base X plus damage-scaled growth;
- vertical launch is upward base Y plus half the damage-scaled growth; and
- validation and runtime saturation keep both components inside the canonical
  motion-speed bound.

On impact, attacker and target enter four frozen ticks. The target retains a
pending launch vector and hitstun duration, then becomes airborne in
`HITSTUN`. Hitstun is the ceiling of the sum of absolute launch components
divided by the data-defined velocity per tick, clamped to 1–600 ticks.

A target enters tumble when that computed hitstun reaches the data-defined
threshold, 32 ticks by default. Jump, attack, fast fall, and ordinary steering
remain locked during hitstun; deterministic gravity and stage collision
continue.

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

- an open window plus neutral horizontal input enters `TECH_IN_PLACE` for 20
  ticks;
- an open window plus horizontal input enters `TECH_ROLL` for 24 ticks in that
  direction at the data-defined roll speed; and
- no open window enters `KNOCKDOWN` for 30 ticks.

These actions currently return directly to ground idle. Invulnerability,
missed-tech get-up choices, attack interruption, and wall/ceiling techs remain
explicit follow-up work and are not implied by these state names.

## Canonical state and inspection

State schema 5 / save format 4 adds per-player tech-window and lockout timers,
digital-trigger edge state, tumble, SDI pulse count and component directions,
and tech-roll direction. The active magic is `PFSAVE04`; the fixed stream is
541 bytes: a 140-byte header plus a 401-byte payload.

Loading validates every new timer, flag, direction, action relationship,
inactive slot, and pending-launch bound before replacing live state. Saving
during hitlag and continuing after load must produce the same per-tick hashes.

Inspection schema 4 exposes percent, hitlag, hitstun, tumble, tech window and
lockout, trigger-held state, SDI count/direction, tech direction, active
hitbox bounds, and last-hit metadata. Browser view schema 3 carries the
reaction fields used by the live state cards.

## Verification

`tests/sim/test_m4_combat.c` and `tools/verify_m4_combat.sh` cover 28 focused
invariants, including:

- attack schedule, facing, whiff, damage, ownership, freeze, launch, hitstun,
  one-hit masks, and simultaneous trades;
- first-component SDI, held-direction rejection, diagonal second-component
  SDI, ASDI/DI launch application, approximate speed preservation, and
  deterministic direction;
- missed tech, in-place tech, directional tech roll, 20-tick window, 40-tick
  lockout, and held-trigger edge behavior;
- rejection of invalid reaction content;
- mid-hitlag save/load plus 80 future equal hashes; and
- a 20,000-tick four-player team trace with a canonical hash after every tick.

The 180-tick replay corpus now includes vertical stick and trigger inputs and
requires observed SDI and tech-window state before encoding. Native and
WebAssembly runs must agree on all 181 state hashes, the 31,233-byte replay,
and its final digest.

The browser startup refuses readiness unless independent movement, attack, and
reaction probes pass. Its reaction probe uses the production hit path to
observe an SDI displacement, then verifies trigger edge, window, and lockout
timers.
