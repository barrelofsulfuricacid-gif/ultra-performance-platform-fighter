# M0 movement representation playtest worksheet

**Status:** Blind playtest complete; owner representation decision pending.

**Purpose:** Compare the subjective movement feel of the float32 and Q16.16
candidates before accepting the M0 representation architecture.

## Setup

Open the browser build linked from `experiments/m0_playtest/README.md`, or
build and run the native `m0_movement_playtest`. Record the seed shown in the
interface:

- Seed:
- Input device:
- Browser and operating system:
- Build configuration: WebAssembly / native release

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

## Recorded result

- Date: 2026-07-27
- Corrected browser prototype:
  `897e5e90c9e96cfd20d38b32c1068fce5ed0c17e`
- Difference perceptible: No
- Preferred candidate based on feel: No preference
- Owner decision: Pending

Scores and environment details were not supplied and are intentionally left
blank rather than inferred.
