# M0 movement representation playtest worksheet

**Status:** Awaiting owner playtest.

**Purpose:** Compare the subjective movement feel of the float32 and Q16.16
candidates before accepting the M0 representation architecture.

## Setup

Build and run `m0_movement_playtest` using
`experiments/m0_playtest/README.md`. Record the seed shown at the bottom of the
window:

- Seed:
- Input device:
- Operating system:
- Build configuration:

Keep the representation labels hidden until both trials are scored.

## Maneuvers

For each candidate, press its focus key, reset, and repeat:

1. Ten rapid left-right dash-dance sequences.
2. Slow analog walks into full-speed runs and immediate reversals.
3. Ten short hops and ten full hops.
4. Aerial drift reversals before and after the jump apex.
5. Double jumps with early and late direction changes.
6. Fast falls at several heights.
7. Landings on the pass-through platform and intentional platform drops.
8. Runs off both stage edges followed by aerial return attempts.

The two simulations always consume the same input. Focus changes visual
attention only.

## Blind scores

Score each applicable dimension from 1 to 5 before pressing `V`.

| Dimension | Candidate A | Candidate B | Notes |
|---|---:|---:|---|
| Input immediacy |  |  |  |
| Ground control |  |  |  |
| Air control |  |  |  |
| Collision/platform stability |  |  |  |
| Movement expression |  |  |  |
| Visual stability |  |  |  |
| Overall fun |  |  |  |

Combat clarity, combo expression, and recovery/edge-play depth are not scored
by this movement-only prototype. They remain mandatory at the M4 combat
checkpoint.

## Reveal and decision

- Candidate A representation:
- Candidate B representation:
- Was any difference perceptible without watching the numeric delta?
- Was the difference repeatable after reset?
- Did either candidate produce a critical snag, jitter, tunneling, or
  unexplained platform contact?
- Preferred candidate: Q16.16 / float32 / no preference / retest required
- Does the preference change when considering the existing throughput,
  snapshot, and determinism evidence?
- Owner decision: approve Q16.16 / approve float32 / request changes and retest

M0 remains open until the owner selects a representation or explicitly records
an exception.
