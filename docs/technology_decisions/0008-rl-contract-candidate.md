# TDR-0008: Reinforcement-learning contract

- **Status:** Accepted by owner and implemented as RL schema 2
- **Date:** 2026-07-27

## Scope

This record defines the C contract exposed by `pf/rl.h`. The owner accepted
raw analog actions, both observation forms with seed redaction, shaped rewards,
and the fixed-stride independent-error batch contract. Those decisions are
implemented in RL schema 2 before later combat, mode, and content work makes
the interface expensive to change.

## Actions

One `pf_rl_action` represents one player for one logical tick:

| Field | Encoding |
|---|---|
| Buttons | Versioned 64-bit bitset shared with normalized player input |
| Main stick | Signed 16-bit x/y |
| Secondary stick | Signed 16-bit x/y |
| Triggers | Unsigned 16-bit left/right |
| Schema | Action schema version plus zero reserved field |

The environment supplies tick and stable player-slot identity. Single stepping
requires exactly the configured player count. Batched actions use a fixed
four-action stride so duel and team environments share one memory layout.

Legal-button masks report the accepted versioned button vocabulary for active
players while an episode can advance, and zero after termination, truncation,
or a deterministic fault. Analog ranges are fixed in `pf_rl_spec`.

## Observations

Every RL transition contains both:

- A structured `pf_sim_observation`, preserving named fields for bindings and
  schema review.
- A flat 36-element signed-32-bit observation for low-overhead contiguous
  transfer.

Both normal RL views redact the reset seed. The structured seed field is zero,
and compact words 2–3 are reserved zero. A debugger or replay tool that
explicitly calls `pf_sim_observe` still receives the diagnostic seed; the
caller of reset also already owns the supplied seed. This prevents a policy
from receiving a convenient episode identifier while preserving deterministic
diagnostics outside the bot observation.

The compact layout is:

| Indices | Values |
|---|---|
| 0–1 | Tick, low/high 32-bit words |
| 2–3 | Reserved zero; seed is not a policy observation |
| 4–5 | Maximum ticks, low/high 32-bit words |
| 6 | Deterministic fault flags |
| 7 | Packed player count, mode, termination, truncation, and winner mask |
| 8–14 | Player 0 previous-button words, position x/y, velocity x/y, packed slot/team/grounded/active |
| 15–21 | Player 1 fields |
| 22–28 | Player 2 fields |
| 29–35 | Player 3 fields |

Bit patterns are copied rather than implementation-defined signed casts.
Inactive slots remain canonical zero except for their implicit packed slot.
Python may expose normalized floating views, but those conversions do not
define or enter simulation.

## Rewards and episode signals

Rewards are signed Q16.16 integers and combine two advertised components:

1. A bounded engagement-potential delta on every successful step.
2. A terminal match-outcome component.

For active player `i`, the provisional M2 engagement potential is:

`potential(i) = -0.25 * clamp(nearest-opponent-x-distance, 0, 128 units) /
128 units`

The step component is `potential(after) - potential(before)`. Opponents are
active players on a different team. The fixed-point implementation uses
integer arithmetic, exposes the `0.25` (`16384` Q16.16) potential limit in
`pf_rl_spec`, and uses a fixed 128-world-unit reference distance so the
per-unit signal does not change with stage width. It considers horizontal
distance only because M2 has no combat state. Closing distance produces a
small positive reward, separating produces a negative reward, and returning
to the same positions has zero undiscounted net shaping reward. This avoids
rewarding raw button presses or velocity that could be farmed without
interaction.

On the transition that deterministically terminates a match, the outcome
component is added to any engagement delta:

- Every winning player receives `+1.0` (`+65536`).
- Every losing player receives `-1.0` (`-65536`).
- A terminal draw/no-winner result gives every player zero.
- A post-terminal step returns `PF_STATUS_EPISODE_DONE` and does not repeat the
  reward.

For teams, a forfeit by one team awards the opposing team mask. Simultaneous
forfeits by both teams have no winner. This makes the provisional two-versus-two
terminal component zero-sum. Reset, invalid operations, and post-terminal
steps produce zero; a time-limit step can still contain its final engagement
delta.

M4 combat work may add damage, stock, or knockout shaping through a new
advertised reward-component/schema version. It must not silently reinterpret
the M2 engagement component.

## Batch behavior

`pf_rl_reset_batch` and `pf_rl_step_batch` are caller-owned, allocation-free
loops over the same reset/tick implementation used by single environments.
Each output transition carries its own status. The batch processes every
environment and returns the first non-OK status, so one malformed action does
not suppress valid independent environments.

## Conformance and open review

`tests/sim/test_rl_api.c` checks:

- Schema/spec metadata and compact/structured correspondence.
- Seed redaction in both normal RL observation forms.
- Reset, approach/separation shaping, and legal masks.
- Atomic invalid-action rejection.
- Duel and team terminal masks/rewards.
- No repeated terminal reward.
- Six-environment batch reset/step, fixed-stride rejection, and independent
  error status.

## Owner decision record

On 2026-07-27 the owner selected:

1. Raw normalized analog actions.
2. Both structured and compact formats, with the seed hidden from normal bot
   observations.
3. Shaped rewards in addition to terminal outcomes.
4. A fixed four-action stride with per-environment status/error behavior.
