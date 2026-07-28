# M4 first combat-primitive contract

## Scope

This checkpoint adds the first production-path M4.2 combat action to the
original placeholder fighter. It establishes the deterministic hit pipeline
that later ground, aerial, special, grab, shield, and recovery actions will
reuse. It does not claim the rest of M4.2 or any of the 61-technique M4
acceptance gate.

The action is entered by a rising edge on the input-schema-2 attack bit while
the fighter is grounded and outside jump squat, landing, hitlag, hitstun, or an
existing attack. There is no universal input buffer. The default data defines
two startup ticks, two active ticks, eight recovery ticks, and four hitlag
ticks. Ground traction continues during the action.

## Collision and ownership

- The fighter content table defines hitbox offset, half extents, damage, base
  launch, damage growth, hitstun conversion, and phase durations.
- The active hitbox is an axis-aligned box mirrored by fighter facing. A
  fighter hurtbox is the existing data-driven body box.
- Each target can be hit once by one execution of the action. Its player bit is
  retained in canonical attack state through active frames and hitlag.
- Self-hits and same-team hits are rejected.
- If multiple legal hitboxes overlap one target on the same tick, the lower
  attacker slot owns that target. Different targets can still be hit on the
  same tick.
- A simultaneous trade resolves both hits. Being hit takes precedence over
  attacker-only hitlag, so each traded fighter resumes in hitstun.

Collision is resolved once after every active player has completed movement
for the tick. This makes ownership independent of the order in which movement
was stepped.

## Damage, hitlag, launch, and hitstun

Damage is unsigned Q16.16 percent and saturates at 999%. The default action
adds 6%. Launch uses the target's post-hit damage:

- horizontal launch is facing-signed base X plus damage-scaled growth;
- vertical launch is upward base Y plus half the damage-scaled growth; and
- validated content and runtime saturation keep both components inside the
  canonical motion-speed bound.

On impact, attacker and target enter four frozen ticks. The target retains a
pending launch vector and hitstun duration in canonical state, then becomes
airborne in `HITSTUN` when hitlag ends. Hitstun duration is the ceiling of the
sum of absolute launch components divided by the data-defined velocity per
tick, clamped to 1–600 ticks. Player control, jump, attack, fast fall, and
aerial steering are ignored during hitstun; normal deterministic gravity and
stage landing collision still apply.

Crossing a blast boundary resets damage and transient attack, hitlag, and
hitstun state with the existing placeholder respawn. The most recent hit
metadata remains diagnostic history until the next hit or full simulation
reset.

## Canonical state and inspection

State schema 4 / save format 3 appends:

- a monotonic combat-event sequence;
- per-player damage and pending launch;
- per-player last-hit sequence, tick, damage, and attacker;
- per-player hitlag and hitstun timers;
- the post-hitlag resume action; and
- the per-attack hit mask.

The format is a fixed 501-byte stream and validates every timer, enum,
relationship, player bit, attacker identity, inactive slot, and pending-launch
bound before atomic load. Saving during hitlag and continuing after load must
produce the same per-tick hashes.

Inspection schema 3 exposes percent, hitlag, hitstun, active hitbox bounds,
attack hit mask, and last-hit metadata. The browser adapter renders active
hitboxes and shows percent/timers without creating presentation-only collision
objects.

## Verification

`tests/sim/test_m4_combat.c` and `tools/verify_m4_combat.sh` cover:

- startup, active, recovery, facing, whiff, and one-hit-per-action behavior;
- damage, hit ownership, hitlag freeze, launch, hitstun control lockout, and
  simultaneous trades;
- rejection of content whose maximum launch would exceed canonical bounds;
- save/load and 80-tick future equality from the middle of hitlag; and
- a 20,000-tick four-player team trace that exercises combat and validates a
  canonical hash after every tick.

The shared 180-tick native/WebAssembly replay corpus now contains real attack
inputs and confirmed hit events. The browser startup refuses readiness unless
its movement probe and a production-path damage/hitlag probe both pass.
