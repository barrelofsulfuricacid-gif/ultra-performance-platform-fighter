# TDR-0006: Canonical state format and hash

- **Status:** Accepted for save format 1
- **Date:** 2026-07-27

## Decision

Save format 1 is a fixed 305-byte, field-by-field little-endian encoding:

| Region | Bytes | Contents |
|---|---:|---|
| Header | 140 | Magic, format/header versions, simulation compatibility versions, tick rate, content hash, configuration hash, tick, payload length, checksum identity, payload checksum |
| Payload | 165 | Every deterministic `pf_world_state` field in schema order, excluding C padding and the separately identified content hash |

The header magic is `PFSAVE01`. The configuration identity is SHA-256 over the
domain `PFCFG001` followed by the canonical configuration fields. The payload
checksum is SHA-256 over the 165 payload bytes. `pf_sim_hash` is SHA-256 over
the complete emitted 305-byte save stream and reports both its algorithm and
algorithm version.

Load parses into a temporary fixed-size world value, validates the complete
header, lengths, compatibility identity, checksum, schema fields, enum values,
flags, player slots, teams, numeric ranges, and inactive-slot invariants, and
only then replaces live state. A failed load therefore leaves the destination
unchanged. Clone applies the same content/configuration compatibility gate and
copies state without serialization.

## Why SHA-256

SHA-256 has a stable public specification in
[NIST FIPS 180-4](https://csrc.nist.gov/pubs/fips/180-4/upd1/final), produces a
full 256-bit digest, and can be implemented with fixed-width integer operations
without a platform or allocation dependency. That makes format compatibility
and native/WebAssembly behavior straightforward.

XXH3-128 from xxHash 0.8.3 was also reviewed. It is a strong candidate if
measured replay-verifier hashing cost becomes material, but adopting it now
would add a third-party source and update surface to the deterministic target.
The versioned hash identifier permits a future format migration with explicit
compatibility handling.

Hashing is opt-in through `pf_sim_hash`; the normal tick path does not hash.
M3 measures hash/snapshot overhead before any policy puts a digest on a
performance-critical path. Ranked replay authentication remains a separate
service-envelope responsibility.

## Conformance

`tests/sim/test_sim_snapshot.c` checks:

- The FIPS SHA-256 `abc` vector.
- The exact format size and representative little-endian offsets.
- Equality between `pf_sim_hash` and SHA-256 of emitted save bytes.
- Save/load/clone equality and equal future evolution after clone.
- Required-size reporting.
- Checksum, version, header, trailing-byte, content, and configuration
  rejection.
- Atomicity of every failed load checked by before/after state hash.

`tools/verify_m2_kernel.sh` compiles and runs this conformance test directly
under the strict C17 warning policy, and includes serialization/hash objects in
the forbidden-symbol inspection.
