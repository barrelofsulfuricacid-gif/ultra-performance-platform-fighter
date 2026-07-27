# Dependency register

Research was refreshed from primary project sources on 2026-07-27. These are
reviewed pins and candidates. An adopted row has a checksum-verified,
repository-local bootstrap path and boundary/conformance evidence; candidate
rows still require their representative adoption work.

| Area | First-spike pin | License | Target role | Status |
|---|---|---|---|---|
| Build configuration | CMake 4.4.0 / `44125a9` | BSD-3-Clause | Host build tool | Adopted in M1 |
| Build executor | Ninja 1.13.2 / `3441b63` | Apache-2.0 | Host build tool | Adopted in M1 |
| Native platform | SDL 3.4.12 / `f87239e` | zlib | Windows/macOS/Linux client | Adopted for M1 client baseline |
| Web toolchain | Emscripten 6.0.3 / `9074aa5` | MIT + NCSA | C/C++ to Wasm | Adopted in M1 |
| Debug GUI | Dear ImGui 1.92.8 | MIT | Developer client only | Selected matched version |
| Generated GUI C API | Dear Bindings 0.21 / `c9ff649` | MIT | Private C++ island | Selected matched version |
| Rollback reference | GGPO repository, observed 2026-07-27 | MIT | Behavioral reference only | Not linked |
| Native WebRTC candidate | libdatachannel 0.24.5 / `443f693` | MPL-2.0 | Native cross-play transport | Candidate |
| ICE candidate | libjuice 1.7.2 / `3c40a35` | MPL-2.0 | Native ICE/UDP | Candidate |
| Workbook candidate | XLSX I/O 0.2.36 / `a9016eb` | MIT | Developer/offline tools | Candidate |
| Audio candidate | miniaudio 0.11.25 / `9634bed` | Public domain or MIT-0 | Native/web client | Candidate |
| Audio candidate | SDL_mixer 3.2.4 / `72a8186` | zlib | Native/web client | Candidate |
| Profiler | Tracy 0.13.1 / `05cceee` | BSD-3-Clause | Profile builds/tools | Selected |
| Performance store | SQLite 3.53.4 / source ID `bf7c7f3…` | Public domain | Local benchmark tools | Selected |
| RL compatibility | Gymnasium 1.3.0 / `53bf3e9` | MIT | Optional Python wrapper | Adopted for M2 adapter |

## Adoption gate

No row may move from candidate/first-spike to vendored or fetched until its
decision record includes:

- Immutable source URL/tag and full archive checksum.
- Complete license/notice bundle and transitive license review.
- Supported target matrix and clean-build evidence.
- Authored C boundary test where applicable.
- Measured representative overhead and binary-size impact.
- Security/update owner and replacement procedure.
- Confirmation that `sim` and the final headless link graph do not acquire the
  dependency accidentally.

The M1 build/platform rows satisfy this gate through
`dependencies/toolchains.lock.tsv`,
`docs/technology_decisions/0002-build-platform-and-web.md`, the clean-machine
CI matrix, and `tools/verify_m1_setup.sh`. The ignored `.toolchains` extraction
is a verified build input, not vendored source.

The Gymnasium row satisfies the same boundary gate through
`dependencies/python.lock.tsv`, TDR-0005, `tools/verify_m2_python.sh`, and the
isolated `rl-python` clean-machine CI job. It is an optional packaging/test
dependency and is absent from every native engine product.

Steam Input is a platform service/API rather than a vendored source dependency.
Its eventual adapter is additive to SDL Gamepad and cannot change normalized
simulation input semantics.
