# M0 movement representation playtest

This disposable prototype closes the human-feel acceptance item in M0. It
applies the same normalized input, 60 Hz stage rules, and movement parameters
to two pure-C candidates:

- IEEE float32 position and velocity.
- Signed Q16.16 fixed-point position and velocity with 64-bit intermediates
  and explicit rounding.

The prototype is deliberately movement-only. It is evidence for the numeric
representation decision, not permanent engine code or a combat vertical slice.

## Browser playtest

The same pure-C movement core is available as an owner-only browser build:

<https://m0-movement-playtest.lol1234.chatgpt.site>

It runs the float32 and Q16.16 candidates as WebAssembly at a fixed 60 Hz and
includes keyboard, controller, touch, blind scoring, reveal, and result-copy
flows. The browser adapter source and deterministic trace verifier live in
[`web/`](web/).

## Build

CMake 3.24 or newer and a C17 compiler are required. SDL 3.4.12 is used when
installed; otherwise CMake downloads the official archive and verifies its
pinned SHA-256 before building it. Linux source builds also require the normal
SDL desktop development dependencies for X11 or Wayland; see the official
[SDL Linux build instructions](https://wiki.libsdl.org/SDL3/README-linux).

```sh
cmake -S experiments/m0_playtest -B build/m0_playtest
cmake --build build/m0_playtest --config Release
ctest --test-dir build/m0_playtest --output-on-failure
```

Run `m0_movement_playtest` from the selected build directory. Multi-config
generators place it in a `Release` subdirectory.

Controls:

- `A`/`D`, arrow keys, or left stick: horizontal movement.
- `Space`, `W`, up, or gamepad south button: jump.
- `S`, down, or gamepad down: fast fall and platform drop.
- `1`/`2`: focus Candidate A/B.
- `R`: reset both candidates; `P`: pause; `N`: single step.
- `T`: toggle trails; `V`: reveal the randomized assignment; `Esc`: quit.

The SDL rendering path has a window-free smoke mode:

```sh
build/m0_playtest/m0_movement_playtest --smoke --seed 20260727
```

The pure-C trace verifier does not require SDL:

```sh
cc -std=c17 -O2 -Wall -Wextra -Wpedantic -Werror \
  experiments/m0_playtest/movement_model.c \
  experiments/m0_playtest/movement_verify.c \
  -lm -o build/m0_movement_verify
build/m0_movement_verify
```

## Blind playtest protocol

The interactive application randomizes which representation is Candidate A
and Candidate B. Do not reveal the assignment until both trials are complete.

1. Focus Candidate A, reset, and perform repeated dash-dances, analog
   walk/run changes, short hops, full hops, aerial reversals, double jumps,
   fast falls, platform landings, and platform drops.
2. Focus Candidate B, reset, and repeat the same maneuvers.
3. Rate each candidate for response, precision, consistency, movement
   expression, visual stability, and fun using the M0 rubric.
4. Reveal the assignment and record whether any difference was perceptible,
   repeatable, and preference-changing.

The candidates always receive the same input even when one is visually
focused. This preserves trace comparability. The on-screen delta is diagnostic
only and should not replace the blind feel rating.
