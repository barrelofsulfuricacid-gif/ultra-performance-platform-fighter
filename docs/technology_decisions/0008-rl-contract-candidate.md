# TDR-0008: Reinforcement-learning contract candidate

- **Status:** Implemented candidate; owner approval required at M2 checkpoint
- **Date:** 2026-07-27

## Scope

This record defines the candidate C contract exposed by `pf/rl.h`. Schema 1 is
deliberately reviewable before later combat, mode, and content work makes it
expensive to change. It is not owner-approved until the M2 human checkpoint.

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

Every transition contains both:

- The structured `pf_sim_observation`, preserving named fields for debugging,
  bindings, and schema review.
- A flat 36-element signed-32-bit observation for low-overhead contiguous
  transfer.

The compact layout is:

| Indices | Values |
|---|---|
| 0–1 | Tick, low/high 32-bit words |
| 2–3 | Seed, low/high 32-bit words |
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

Rewards are signed Q16.16 integers. Normal ticks and time-limit truncation
produce zero. On the transition that deterministically terminates a match:

- Every winning player receives `+1.0` (`+65536`).
- Every losing player receives `-1.0` (`-65536`).
- A terminal draw/no-winner result gives every player zero.
- A post-terminal step returns `PF_STATUS_EPISODE_DONE` and does not repeat the
  reward.

For teams, a forfeit by one team awards the opposing team mask. Simultaneous
forfeits by both teams have no winner. This makes the provisional two-versus-two
terminal reward zero-sum.

## Batch behavior

`pf_rl_reset_batch` and `pf_rl_step_batch` are caller-owned, allocation-free
loops over the same reset/tick implementation used by single environments.
Each output transition carries its own status. The batch processes every
environment and returns the first non-OK status, so one malformed action does
not suppress valid independent environments.

## Conformance and open review

`tests/sim/test_rl_api.c` checks:

- Schema/spec metadata and compact/structured correspondence.
- Reset, movement, legal masks, and zero nonterminal reward.
- Atomic invalid-action rejection.
- Duel and team terminal masks/rewards.
- No repeated terminal reward.
- Six-environment batch reset/step, fixed-stride rejection, and independent
  error status.

The owner checkpoint must explicitly accept or request changes to:

1. Raw normalized analog actions versus a discrete action vocabulary.
2. Structured plus 36-word compact observations, including exposed seed.
3. Sparse terminal-only `+1/-1` rewards versus additional shaped rewards.
4. Fixed four-action batch stride and per-environment error behavior.
