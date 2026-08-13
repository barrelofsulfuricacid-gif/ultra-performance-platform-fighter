# TDR-0008: Reinforcement-learning contract

- **Status:** Accepted by owner; current implementation is RL schema 14
- **Date:** 2026-08-01

## Scope

This record defines the C contract exposed by `pf/rl.h`. The owner accepted
signed analog actions, both observation forms with seed redaction, shaped rewards,
and the fixed-stride independent-error batch contract. Those decisions are
implemented in RL schema 2 before later combat, mode, and content work made
the interface expensive to change. RL schema 3 preserved those decisions and
added the M4 stock, respawn, invulnerability, and sudden-death observations.
Historical RL schema/transition schema 4 retained the same action, observation,
reward, and batch semantics while embedding the then-current ABI-4 per-tick
event journal in every transition result. Later compatible revisions expose
the fixed item slot, projectile slot, per-player special charge, per-player
recovery availability, per-player smash charge, raw shield strength, shield
health, and shield tilt.
The current contract is RL schema 14, action schema 1, transition schema 12,
structured observation schema 13, and compact observation schema 13. Its
transition result embeds the ABI-5 event journal.

## Actions

One `pf_rl_action` represents one player for one logical tick:

| Field | Encoding |
|---|---|
| Buttons | Versioned logical-button bitset shared with normalized player input |
| Main stick | Signed 16-bit x/y |
| Secondary stick | Signed 16-bit x/y |
| Triggers | Unsigned 16-bit left/right |
| Schema | Action schema version plus zero reserved field |

The environment supplies tick and stable player-slot identity. Single stepping
requires exactly the configured player count. Batched actions use a fixed
four-action stride so duel and team environments share one memory layout.
The action schema deliberately exposes processed Q15 axes, not the input-schema
6 raw PADStatus bytes or validity mask. RL stepping therefore uses the core's
deterministic processed-axis-to-raw fallback. It follows the same production
simulation path, but is not an authoritative raw-controller UCF evidence lane.

Legal-button masks report the accepted versioned button vocabulary for active
players while an episode can advance. A player waiting to respawn or already
eliminated exposes only forfeit; all masks become zero after termination,
truncation, or a deterministic fault. Analog ranges are fixed in `pf_rl_spec`.

## Observations

Every RL transition contains both:

- A structured `pf_sim_observation`, preserving named fields for bindings and
  schema review.
- A flat 102-element signed-32-bit observation for low-overhead contiguous
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
| 7 | Packed player count (bits 0–7), mode (8–15), termination (16), truncation (17), sudden death (18), configured stock count (19–25), and winner mask (26–29) |
| 8–17 | Player 0 previous-button words, position x/y, velocity x/y, packed slot/team/grounded/active/recovery-ready/prone-orientation flags, stocks remaining, respawn ticks, and respawn-invulnerability ticks |
| 18–27 | Player 1 fields |
| 28–37 | Player 2 fields |
| 38–47 | Player 3 fields |
| 48–55 | Fixed item position/velocity, packed lifecycle/ownership fields, and timers |
| 56–61 | Fixed projectile position/velocity and packed lifecycle/owner fields |
| 62–65 | Per-player Arc Reservoir charge ticks |
| 66–69 | Per-player smash-charge ticks |
| 70–73 | Per-player raw shield strength; zero outside shield, shield stun, or hitlag resuming into shield stun |
| 74–77 | Per-player canonical shield health in Q16.16 |
| 78–85 | Per-player signed x/y shield tilt; two consecutive values per fixed player slot |
| 86–101 | Four four-value stale-move records: current/resume move multiplier in Q16.16; count in bits 0–7 plus IDs 0–2 in bits 8–31; IDs 3–6; then IDs 7–8 with high bits reserved zero |

The stale multiplier describes the player's current attack, or the attack that
will resume after hitlag; it is exactly one for a state without a stale-capable
move. Queue entries beyond the advertised count are zero. The per-instance
registration latches remain inspection/debug state and are not policy inputs.
Prone orientation uses bits 19–20 of each packed player-flags word: 0 is none,
1 is back, and 2 is stomach. The structured player record exposes the same
enum directly; compact vector length remains 102.

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
per-unit signal does not change with stage width. It remains the provisional
M2 horizontal-distance contract even though M4 now has an initial combat
state. Closing distance produces a small positive reward, separating produces
a negative reward, and returning to the same positions has zero undiscounted
net shaping reward. This avoids rewarding raw button presses or velocity that
could be farmed without interaction.

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

M4 combat work may add damage, stock-loss, or knockout shaping through a new
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
- Stock, respawn, invulnerability, sudden-death, and winner-bit
  compact/structured correspondence.
- Prone none/back/stomach structured state and player-flags bits 19–20.
- Item, projectile, Arc Reservoir charge, smash charge, raw shield strength, and
  Vector Ascent recovery availability in the structured/compact contract;
  recovery remains player flag bit 18.
- Light-threshold RL input enters shield, preserves exact strength in both
  observation forms, and uses the same production tick path as native input.
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
