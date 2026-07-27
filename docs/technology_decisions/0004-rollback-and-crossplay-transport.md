# TDR-0004: Rollback model and native/browser transport

- **Status:** Rollback interface accepted; transport choice deferred to M9
- **Date:** 2026-07-27
- **Implements:** D4-A and D5-A

## Rollback decision

Author the rollback orchestration in C against the simulation save/load/hash
and input-frame contracts. Use GGPO's behavioral model and callback concepts as
a reference, not as a linked dependency.

The upstream GGPO repository has no releases and documents Windows-only builds.
That makes it unsuitable as the cross-platform/native-WebAssembly authority
without a port and conformance proof. It is MIT licensed.

Evidence:

- [GGPO repository, model, platform note, and license](https://github.com/pond3r/ggpo)
- [GGPO rollback overview](https://www.ggpo.net/)

## Transport candidates

The native first-spike pair is:

- libdatachannel 0.24.5 (`443f693`, MPL-2.0), which provides C bindings,
  browser-compatible WebRTC DataChannels, and a larger C++/dependency surface.
- libjuice 1.7.2 (`3c40a35`, MPL-2.0), a dependency-free C ICE library that can
  support a smaller custom UDP path but does not by itself supply the browser
  DataChannel protocol.

The browser uses `RTCDataChannel` through a narrow JavaScript adapter. Full
native/browser cross-play requires a protocol compatible with browser security,
ICE, DTLS, SCTP/DataChannel, STUN/TURN, and deployment constraints; raw browser
UDP is not assumed.

Evidence:

- [libdatachannel 0.24.5 release](https://github.com/paullouisageneau/libdatachannel/releases/tag/v0.24.5)
- [libdatachannel C bindings, targets, dependencies, and license](https://github.com/paullouisageneau/libdatachannel)
- [libjuice 1.7.2 release](https://github.com/paullouisageneau/libjuice/releases/tag/v1.7.2)
- [libjuice targets, capabilities, and license](https://github.com/paullouisageneau/libjuice)
- [RTCDataChannel](https://developer.mozilla.org/en-US/docs/Web/API/RTCDataChannel)

## M9 selection test

Compare candidate paths with identical encrypted unordered/unreliable binary
traffic across:

- Native↔native, browser↔browser, and every native↔browser pair.
- Direct LAN, typical NAT, symmetric NAT/TURN fallback, IPv4, and IPv6.
- Controlled latency, jitter, reordering, duplication, and loss.
- Channel establishment time, packet overhead, p50/p95 delivery latency,
  CPU/memory, binary size, idle behavior, reconnect, and failure diagnostics.

The transport only moves bytes. It does not own prediction, frame numbering,
rollback, replay, ranking, or simulation state.

## Ranked verification

P2P peers submit an authenticated replay/input envelope. Rating remains pending
until a server worker:

1. Resolves the exact accepted headless build and content pack.
2. Re-simulates every input.
3. Verifies checkpoints, final state, result, and protocol invariants.
4. Rejects disagreement, malformed data, incompatible versions, or policy
   violations.
5. Atomically finalizes the rating update and immutable match record.

Transport or replay signatures cannot make a modified client authoritative.

## Replacement plan

Transport adapters implement open/close/send/receive/state/error and expose
bytes to the authored rollback layer. Either candidate can be replaced without
changing simulation, replay, or service schemas.
