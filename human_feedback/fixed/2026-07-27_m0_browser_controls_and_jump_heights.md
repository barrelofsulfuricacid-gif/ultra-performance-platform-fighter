# M0 browser movement controls and jump heights

**Status:** Fixed

**Reported:** 2026-07-27

**Fixed commit:** `897e5e90c9e96cfd20d38b32c1068fce5ed0c17e`

## Human feedback

1. The browser keyboard controls could not produce a slow walk or a usable
   dash dance.
2. Short-hop and full-hop height varied continuously with jump-button hold
   duration instead of selecting one fixed height during jumpsquat.

## Resolution

- Full-strength direction input now enters a ten-tick initial dash window, and
  an opposite full-strength input during that window pivots immediately.
- `Shift+A/D` emits walk-strength input; unmodified `A/D` emits full-strength
  input.
- Releasing jump during the three-tick jumpsquat latches a fixed short-hop
  velocity. Holding through takeoff latches a fixed full-hop velocity.
  Releasing after takeoff no longer cuts vertical velocity.
- The float32 and float32 candidates implement the same new state transitions.

## Verification

- Native C verifier: seven focused cases and a deterministic 7,200-tick trace.
- WebAssembly verifier: slow-walk threshold, initial dash, pivot reversal,
  binary short/full-hop apexes, replay equality, and the 7,200-tick trace.
- Generated Wasm SHA-256:
  `cc33c22d9a3162d5d805563a14bf639d0cd99b947ff051ba6aee70aece99389c`.
- Corrected owner-only browser checkpoint:
  <https://m0-movement-playtest.lol1234.chatgpt.site>
