# Locked toolchains and dependencies

`toolchains.lock.tsv` is the machine-readable M1 archive lock. Every network
download used by bootstrap has an immutable version/revision, byte length, and
full SHA-256 digest. Tab-separated fields keep the same file directly readable
from POSIX shell and PowerShell without adding a parser dependency.

`python.lock.tsv` separately records the optional Gymnasium source used to
qualify the M2 Python adapter. Gymnasium never enters the engine bootstrap or
the native/headless link graph.

The locked host tools are CMake 4.4.0 and Ninja 1.13.2. The web lane pins:

- Emsdk commit `db04e88298d9916fc51fcd3743045ca3eb695127`.
- Emscripten 6.0.3 release revision
  `9074aa513b501925adb1361e208932ad32a29a5f`.
- Node.js 22.16.0, which the pinned emsdk release declares.

The bootstrap pre-downloads and verifies both Emscripten payloads before
allowing emsdk to extract them. This closes emsdk's own missing archive-hash
check. The host bootstrap also downloads, verifies, and extracts the locked SDL
3.4.12 source archive into `.toolchains/dependencies`; native client presets
build it statically without changing a system installation.

The same bootstrap installs the official SQLite 3.53.4 amalgamation under
`.toolchains/dependencies`. Only the benchmark/history product compiles this
source. The deterministic simulation and maximum-throughput headless product
do not link SQLite.

Tracy 0.13.1 is likewise checksum-locked and installed under
`.toolchains/dependencies`. Its C++ client is enabled only for the `profile`
configuration and linked only to the benchmark executable. Release,
benchmark, verifier, and maximum-throughput headless products contain no Tracy
client or zone instrumentation. `tools/capture_profile.sh` builds Tracy's
matching command-line capture utility from that source without installing it
system-wide. The capture-only Capstone 6.0.0-Alpha5, PPQSort 1.0.6, and Zstd
1.5.7 sources are locked independently so Tracy's generic build scripts never
perform an unverified transitive download.

## Native compiler lanes

M1 CI uses the non-preview `ubuntu-24.04`, `windows-2025`, `macos-15-intel`,
and `macos-15` runner labels. The accepted compiler compatibility lanes are:

- GNU C 13.3.x on Ubuntu.
- Clang 17.0.x on macOS.
- MSVC 19.44.x (`vcvars_ver=14.44`) on Windows. The compatibility toolset is
  selected explicitly even when the hosted runner's Visual Studio shell is
  newer.

Bootstrap validates the lane rather than silently accepting a different
compiler. The full compiler patch, runner image version, operating system,
architecture, and flags remain part of every performance compatibility key.
Changing a lane or archive is an explicit reviewed lock-file change.

## Bootstrap contract

POSIX hosts use `./tools/bootstrap.sh`; Windows uses
`.\tools\bootstrap.ps1`. Both commands install repository-local host tools,
validate the compiler lane, configure the project hook, and run the headless
smoke. Pass `--web` on POSIX or `-Web` on Windows to install and smoke the
locked web toolchain as well.

The matching `tools/workflow.sh` and `tools/workflow.ps1` commands run the same
named CMake workflows. No bootstrap modifies a system installation or shell
profile. Downloads use a temporary partial name and become eligible for
extraction only after both their byte length and SHA-256 match this lock.

The native Linux workflow additionally needs the operating system's standard
X11 SDK headers. CI installs `libx11-dev`, `libxcursor-dev`, `libxext-dev`,
`libxfixes-dev`, `libxi-dev`, `libxrandr-dev`, `libxss-dev`, and
`libxtst-dev` from its recorded Ubuntu runner image before bootstrap. These are
platform SDK inputs, not linked project runtime dependencies; SDL loads the
host X11 libraries dynamically.

## Upgrade procedure

1. Refresh the decision record from primary release sources.
2. Update one component/version at a time.
3. Download every supported archive and compute its SHA-256 locally.
4. Update archive byte lengths and hashes in the lock.
5. Run native and web clean-machine CI plus boundary/link checks.
6. Record licenses, transitive changes, measured size/overhead, security owner,
   and replacement result before changing dependency status.
