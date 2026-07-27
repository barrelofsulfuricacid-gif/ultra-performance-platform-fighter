# M0 browser movement playtest

Open the owner-only hosted playtest:

<https://m0-movement-playtest.lol1234.chatgpt.site>

The browser build preserves the native checkpoint boundary:

- `../movement_model.c` and `../movement_model.h` remain the canonical pure-C
  float32 and Q16.16 implementations.
- `native/movement_web.c` is a narrow C ABI that randomizes Candidate A/B and
  exposes movement state without moving simulation policy into JavaScript.
- `public/movement_core.wasm` is compiled from those C sources. Its adjacent
  manifest records the compiler, source hashes, binary size, and SHA-256.
- `app/page.tsx` and `app/globals.css` mirror the deployed browser adapter,
  including keyboard, gamepad, touch, blind scoring, reveal, and result-copy
  flows.

## Rebuild and verify the WebAssembly core

Node.js 22 or newer is required. Install the pinned WebAssembly-hosted Clang
package, then run:

```sh
cd experiments/m0_playtest/web
npm ci
npm test
```

The verifier runs the same 7,200-tick trace as the native M0 verifier. Expected
output includes:

```text
wasm-self-test=pass trace_ticks=7200 max_position_delta=0.001472473
```

The hosted page uses a thin Vinext/React shell around these checked-in adapter
sources. The engine remains C-first; the web layer is a disposable M0
playtesting surface.
