# SSBM executable-oracle acceleration

## Decision

Use the maintained ExiAI 0.2.0 Slippi build and libmelee 0.47.2 as the default
Linux/WSL executable-oracle runner. It runs headless, uses Dolphin's null video
backend with audio disabled, supplies controller input through EXI, and enables
the project's game-side fast-forward path. Keep each match configuration and
its candidate boundaries in one trace so Dolphin is not relaunched per
candidate.

The owner-supplied `GALE01` NTSC-U 1.02 image remains outside the repository.
The release AppImage and extracted toolchain also remain under ignored
`build/oracle-toolchain/` paths.

## Prior-art sweep

The selected prior art is:

- Vlad Firoiu's maintained [libmelee fork](https://github.com/vladfi1/libmelee)
  at inspected revision `bce21f09984b286e6d36bfd2939e4cd4691f94c2`. Its
  upstream documentation explicitly provides EXI inputs, game-side
  fast-forward, null video, a separate process that keeps the ENet connection
  alive, and an instant-match-restart code.
- The matching [ExiAI 0.2.0 release](https://github.com/vladfi1/slippi-Ishiiruka/releases/tag/exi-ai-0.2.0)
  and [source branch](https://github.com/vladfi1/slippi-Ishiiruka/tree/exi-ai-rebase).
  The release identifies itself as Faster Melee/Slippi 3.5.1 ExiAI. Its Linux
  AppImage SHA-256 is
  `87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd`.
- Current [Dolphin](https://github.com/dolphin-emu/dolphin) command-line/null-
  backend support was reviewed, but a current emulator is not an equivalent
  replacement for the pinned Slippi 3.5.1 oracle without a separate
  qualification.

A project-authored Slippi 3.5.1 NoGUI/software-OpenGL build was prototyped
before the broader sweep. Its 601-row run took 38.15 seconds and was rejected.
Profiling the selected ExiAI path later proved that cold menu setup and
one-frame Python/DME lockstep still dominated the edit loop. ExiAI already
contains `SlippiSavestate`, the game-memory rollback primitive used by its
online path, so the retained native change is a small patch on pinned revision
`bf1aec4de4856eab412996137287f447daa8ae17`, not a second emulator design.

The sweep found no maintained external experiment server that supplies this
checkpoint protocol. `tools/ssbm_exiai_checkpoint.patch` adds an atomic,
process-owned save/load control channel to NoGUI. It preserves the existing
ENet observer, services requests only while the game is blocked for EXI input,
and exposes sixteen fixed snapshot slots. Protocol v2 assigns divergent cases
one immutable slot each and performs the load on a hidden neutral EXI boundary
before submitting the first recorded input. The original immediate-rebase
prototype was deterministic for linear routes but was retired after a
branching special-action pack proved that it could retain state from the
discarded branch.

## Reproducible setup

From WSL at the repository root:

```sh
tools/bootstrap_ssbm_exiai_oracle.sh
```

The bootstrap downloads and verifies the release artifact, extracts it once,
creates an ignored Python environment, pins `melee==0.47.2` and
`dolphin-memory-engine==1.3.1`, and validates the launcher identity. It never
downloads, copies, or modifies the game image.

The warm checkpoint runner additionally requires a source build:

```sh
tools/bootstrap_ssbm_checkpoint_oracle.sh
```

That script verifies the published artifact and Python packages as above,
checks out the pinned ExiAI revision, applies the repository-owned patch, and
builds only the NoGUI target. Its output remains ignored under
`build/oracle-toolchain/exiai-checkpoint/`.

An accelerated capture uses the generated paths:

```sh
build/oracle-toolchain/exiai-python/bin/python \
  tools/capture_ssbm_movement.py \
  --dolphin build/oracle-toolchain/exiai-checkpoint/Binaries/dolphin-emu \
  --oracle-release-artifact \
    build/oracle-toolchain/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage \
  --iso /owner/path/to/GALE01.iso \
  --output build/oracle/capture-exiai.json \
  --common-hurt-geometry-only --memory-probe-hitbox --oracle-exiai \
  --oracle-checkpoint-pack \
  --oracle-coverage-manifest tools/ssbm_falcon_common_hurt_coverage.json
```

For acceleration qualification, run the same extracted launcher and Python
environment without `--oracle-exiai` or `--oracle-release-artifact` to produce
the unaccelerated control. Then run:

```sh
build/oracle-toolchain/exiai-python/bin/python \
  tools/verify_ssbm_oracle_acceleration.py \
  build/oracle/capture-control.json \
  build/oracle/capture-exiai.json
```

## Qualification and measurement

The current qualification trace contains 4,198 rows and twelve Falcon common-hurt
tracks: Initial Dash, RunBrake, CrouchStart, CrouchEnd, KneeBend, SpotDodge,
RollForward, RollBackward, AirDodge, FallSpecial, LandingFallSpecial, and
ordinary Landing,
including physical hit/miss
controls. The automated A/B comparison
strictly matches every requested and observed input, game frame, initialized
active action frame, position, velocity, damage, hitlag, collision decision,
and qualified geometry observation. It covers 1,773 non-standing Falcon rows
and 1,149 non-standing opponent rows. Of those, 1,909 Falcon and 1,120 opponent rows are
initialized, non-hitlag, action-owned pose samples whose complete capsule
geometry also matches exactly. The unaccelerated control SHA-256 is
`32a0a742012f360c1e49b27d2fb2023e16eac5af23694b032a3777d41ad16a9d`;
the current regenerated accelerated capture is
`3d1d6b0047fadc3dc53cef830f0784216e8967f0e7424a08736ca787bec26de6`.

Moving hit/hurt geometry uses a separate headless/null/unlimited lane with
display-bone fast-forward disabled. Fast-forward can skip a display-bone
evaluation that supplies the previous endpoint for a state-3 moving hit sphere,
so equal current poses are not sufficient qualification. Two independent
2,974-row runs are compared with `--same-runner-repeat`: raw process addresses
and idle/setup phase are excluded, while every route, action/frame, hit sphere,
hurt capsule, and previous/current geometry identity remains strict.

Measured on the local WSL host:

| Runner | Wall time | Result |
|---|---:|---|
| stock Windows Slippi 3.5.1 / libmelee 0.40.1, 601 rows | about 28.3 s | baseline |
| project-authored WSL NoGUI/software OpenGL, 601 rows | 38.15 s | rejected |
| ExiAI unaccelerated control, headless, 650 rows | 22.17 s | qualification control |
| ExiAI headless/null/fast-forward, 650 rows | 16.68 s | selected |

Those timings are the retained 650-row benchmark. The current 4,198-row route
adds safely isolated SpotDodge, both-roll, AirDodge, FallSpecial, and
LandingFallSpecial and ordinary Landing collision controls. Its
unaccelerated and accelerated outputs remain field-for-field identical without
changing the selected runner.

The separate 329-row defense-state movement route is also qualified against a
same-binary unaccelerated control. Accelerated SHA-256
`d9dfebcb6e42f5e71ece08490429b61083f81bee067def379b5fdd6270d96b95`
and control SHA-256
`d78abcfe3d252d0f87409aba3343cd838efb739d6311494d520f2f076eb5255f`
compare field-for-field across all rows, including the new above-walk-speed
horizontal LandingFallSpecial route.

The memory probe dominates this geometry-heavy trace, so fast-forward's
measured benefit is smaller than in ordinary state-only traces. The selected
path still runs a larger capture about 41% faster than the previous stock runner,
removes GUI launches, and supplies frames as quickly as the blocking observer
can consume them. Two upstream live tests, each launching its own match, took
7.92 seconds total on the same host.

The latest 4,198-row ordinary-Landing qualification took 37.6 seconds in the
accelerated candidate and 45.3 seconds in the same-binary control. Although the
outputs match exactly, that throughput is rejected for the ongoing edit loop.
Profiling the observation architecture found that the geometry reader performs
hundreds of `process_vm_readv` calls per fighter frame: direct fighter fields
are read separately, and each hurtbox endpoint performs twelve individual
floating-point reads. Emulator fast-forward cannot remove that observer cost.

The replacement architecture keeps one warm Dolphin process and executes many
short cases with checkpoint restoration between them. Direct fighter state and
embedded hurtbox records are read as contiguous snapshots, unique bone matrices
are coalesced, and only fields declared by the case manifest are serialized.
Stored authoritative traces cover the ordinary no-Dolphin edit loop. The
post-build targets remain at most 2 seconds for that local suite, 3 seconds for
a warm changed-domain live run, and 10 seconds for the complete warm Falcon
pack. A single state-leaking mega-scenario is deliberately rejected: it
obscures failures and permits earlier state to contaminate later
qualifications.

The first complete checkpoint pack reduces the common-hurt route from 4,198 to
283 serialized rows and from 26 repeated setup/collision cases to eight
isolated cases.
It still captures all 255 imported action/frame poses across Initial Dash,
RunBrake, CrouchStart, CrouchEnd, KneeBend, SpotDodge, both rolls, AirDodge,
FallSpecial, LandingFallSpecial, and ordinary Landing. One live Dash hit/miss
pair qualifies the end-to-end collision integration. Repeating a long physical
route for every other pose was removed as duplicate evidence: their boundaries
are evaluated from the exhaustive captured capsules and the hash-pinned decomp
collision routine.

The packed command stream still executes every input tick required to reach
the qualified actions, but it snapshots and serializes only declared action
poses and the live discriminator. Redundant shield dwell, terminal action
holds, and post-action recovery ticks are removed. The ordinary-Landing route
remains a native jump and landing; the optimization does not synthesize its
action state.

Two independent runs produced the same canonical pose digest,
`3a1b182dc64ee6db6caa7cc316c633e3330a9001344ca88f5cd57a441b48cdf1`,
and identical live margins: `+0.289212401` for the hit and `-0.156798480` for
the miss. Compared with the accepted 4,198-row artifact, all 255 poses pass the
documented float32 comparison; 24 poses contain 30 component differences and
every difference is exactly one float32 least-significant bit. Five fully
verified warm runs take 2.635-2.729 seconds and pass the manifest's
three-second changed-domain budget. Lifecycle instrumentation
showed that the apparent 18-21-second cold invocation was not primarily
emulation: about nine seconds were spent re-hashing the unchanged 1.4-GB disc
image. Oracle-input digests are now cached atomically against device, inode,
size, and mtime; any change forces a complete re-hash, while verified hardlinks
reuse the same digest. After the one-time
qualification, an unchanged capture lifecycle measures 7.27-8.13 seconds
including process launch, menus, capture, and cleanup.

Large sharded packs also avoid libmelee's redundant executable-version probe.
`Console` normally launches `dolphin --version` in every worker constructor.
The parent now probes once before `fork` and reuses that immutable result only
when the worker executable has the same device/inode/size/mtime fingerprint as
the hash-qualified launcher. The 19-case ledge pack then passes three fresh
runs at 9.649, 8.924, and 9.614 seconds warm under its enforced 10.0-second
budget, with identical 558-row / 514-sample semantic digest. This optimization
does not weaken executable identity or apply to a different filesystem
revision.

Linux ExiAI observations use Dolphin's existing read-only MEM1 shared-memory
mapping behind the same memory-engine interface, eliminating per-field
`process_vm_readv` calls without duplicating capture logic. The NoGUI control
poll is 5 ms instead of the upstream 100 ms; eight checkpoint acknowledgements
take about 0.075 seconds in aggregate. A separate reconnect experiment was
rejected: controller pipes reconnect, but a new Slippi client resumes with a
desynchronized event stream and then blocks. Persistence therefore stays
inside one connected runner/packed invocation rather than relying on unsafe
cross-process reattachment.

The v2 boundary is intentionally unrecorded. A restore request, neutral
controller sample, and acknowledgement complete first; only then are direct
memory overrides and the case's first input applied. This prevents both the
controller sample and placement/RNG writes from targeting the pre-restore
branch. A repeated-load probe protects one-slot reuse, while domains with
divergent action flows can declare up to sixteen one-shot slots. The current
sixteen-slot pack captures 188 special-acquisition rows in 2.529-3.092 seconds
warm inside one connected process.

Natural source-state setup can remain outside the recorded theorem. The
teeter-special pack uses four immutable slots, then performs an unrecorded
controller-only low walk and neutral release to enter actual Ottotto before
each B edge. It captures 28 retained rows in 0.245-0.268 seconds warm and
repeats byte-identically. Production mirrors the source contract with a
velocity-aware ordinary-walk pre-roll against a tiny floor, not a direct state
write or a fixed-tick approximation. This keeps the oracle compact while
preserving the causal gameplay transition being qualified.

Physical two-player checkpoint fixtures must reset both fighters symmetrically.
Overriding visible position alone is insufficient: current, previous, and
collision-owned position fields can retain the restored branch and alter the
contact frame. The GuardOff acquisition pack resets all of those fields for
both Falcons, disables EXI batching, begins the defender's shield with the
causal attack input, and reaffirms it from the opponent-owned Jab action/frame
rather than a host poll count. Waiting until that observed frame to begin
shielding produced intermittent ordinary hits despite a fired edge. Its
17 cases are therefore split into six concurrent workers with no more than
three divergent physical cases per worker; two 119-row captures repeat
byte-identically in 7.607-7.867 seconds warm and 7.937-9.103 seconds cold.
The shared merger retains manifest-declared `setup/edge/observe` suffixes and
rejects missing/duplicate cases. Opponent-owned conditional edges, route-
selected workers, and suffix projection are shared capture infrastructure,
not Falcon-only timing paths.

## Manifest-selected stored regression lane

The ordinary edit loop now has a generic no-Dolphin runner:

```sh
python tools/verify_ssbm_stored_equivalence.py \
  --build-dir build/wsl-stored-equivalence-release --all
```

On Windows, use `build/windows-msvc-release`. `--changed-file PATH` and
`--changed-from REVISION` select only affected domains. Shared simulation,
test, capture, collision, or manifest infrastructure selects every registered
domain; a character-specific imported table or verifier selects only that
character/domain. An unrelated documentation path selects none and skips the
replay corpus.

`tools/ssbm_equivalence_manifest.json` is the character-independent registry:
it owns the two-second budget, shared dependency patterns, domain-manifest
list, and deterministic replay goldens. Each domain manifest owns only its
source identity, action/frame coverage, C identifier bindings, generated
output, production digest, runner command, and physical cases. The generic
Python selector, generator, and C runner contain no Falcon action mapping.
Adding a character registers another domain manifest and supplies only thin
production pose/runtime/geometry adapters.

The shared pose-domain runner also supports manifest-owned pose-facing cases
for source phases where gameplay facing and display-owned collision facing are
not identical. The generic case owns source-frame mapping and the expected
phase tuple; a thin production adapter transforms the real pose accessor. This
keeps phase regressions in the fast domain without copying character capsules
or adding character logic to the runner.

For `falcon-common-hurt`, generation validates twelve complete tracks, 255
poses, eleven capsules per pose, runtime-to-source frame mapping, unique case
IDs, and 20 hit/miss controls. The filtered C runner hashes the capsules
returned by the production accessor under the declared little-endian
serialization and requires production SHA-256
`33e7ceea1447113256972a719f3abc981857d6a0cd67432842100b74dc50a613`.
It also reports the independently pinned live-source pose digest
`3a1b182dc64ee6db6caa7cc316c633e3330a9001344ca88f5cd57a441b48cdf1`.
The selector rejects stale generated C, mismatched counts/digests, any failed
boundary, changed replay output, missing executables, and budget overruns.

Five warm post-build runs measured 116.845-120.355 ms on native Windows MSVC
Release and 148.121-166.786 ms in WSL GCC 13.3 Release. The manifest digest was
identical on both platforms:
`270f7e71a30500401ac97c18ced42e341f89a75443b5482bfaca343d5c642326`.
The complete combat plus focused CTest lane took 0.41 seconds on Windows and
0.44 seconds in WSL. These results meet the two-second post-build target with
substantial margin. They do not replace live Dolphin: changing a pinned source
truth or golden requires a fresh checkpoint capture, live verification, and
provenance review.

Wall-clock data is emitted on stderr and is not part of authoritative JSON.
Idle animation phase can vary between otherwise equivalent boots, so the
verifier pins the ordered action/frame/float32 payload and explicit physical
discriminator instead of hashing incidental idle rows.

The registry currently contains 31 domains and 186 cases. Independent domain
generation and execution run concurrently; each native-CSV domain also runs
its independent cases concurrently, and both levels restore manifest order
before counting or hashing. The preceding 30-domain / 174-case snapshot passed
the two-second guard in 1178.830/1319.197/1471.076 ms on native Windows and
919.397/986.464/1270.306 ms in WSL. Its focused Run-to-RunBrake domain passed
in 263.089 ms and 403.007 ms respectively. The added 12-case Turn-interrupt
domain's three focused current runs take 304.855/347.483/307.646 ms on Windows
and 415.305/458.985/376.784 ms in WSL. The complete current registry plus replay
takes 1706.950/1922.805/1485.200 ms in three isolated Windows runs. One cold
WSL run takes 5.8 seconds; the following three isolated warm runs take
1127.462/1220.305/1096.947 ms. The current registry manifest SHA-256 is
`ffb5f801f55e24ce9c7a94fcbc627e46b1bcf0a42ebb79db37d746a1c9938664`.
Numeric C cases may narrow a domain's inherited serialized-field mask when a
physical setup intentionally isolates only part of the response; the generated
C always writes an explicit zero for inherited masks so GCC and MSVC apply the
same zero-cost representation.

The released-Damage/DamageFall air-dodge domain demonstrates this sparse
numeric-C route. Its two physical Dolphin cases retain 68 source rows, but the
stored gate needs only four action/tumble samples to protect the discrete
callback result: ordinary non-tumble Damage accepts fresh L into AirDodge,
whereas DamageFly/DamageFall rejects it and remains tumbling. Source semantic
SHA-256 is
`7ce52b784989e56f7539b79dd779eed94ab41e4bcd624b980c263af0b916084b`;
production canonical SHA-256 is
`cec3d2b1d9b67ad906bf68b074c8975f6e53bf48cc714650c6498bef7aeba93e`.
Warm capture work takes 0.587707/0.897459 seconds, and the second complete
emulator lifecycle takes 4.981875 seconds. The updated native suites pass
40/40 serial CTests on Windows in 8.42 seconds and 42/42 in WSL in 9.82
seconds.

The Run-to-RunBrake domain demonstrates the same native-CSV route for a natural
movement callback boundary. Four immutable source checkpoints retain 127 rows:
straight down from Run enters RunBrake, radial-gate diagonal down remains Run
for the edge row, down from RunBrake enters CrouchStart, and locked post-TurnRun
Run rejects down. Normalized source SHA-256 values are
`72a9ce8c19948d468f6aea484b72db3b1f0c280846adc4d5677e4c6a20b810fe`
for `ftCo_Run.c`,
`0c75e6a95319f2be3a42dcade65b07671d47d7a31e7191e04cb617fce13866bb`
for `ftCo_RunBrake.c`, and
`80c2e71e50622e942754bfcdd3bd89f3762fe4df2400d8055f059ab6cc4b8082`
for `ftCo_Squat.c`. The two live captures have identical ordered rows and raw
artifact SHA-256 values
`1d3c568f38f6dcd359e77c3b1616a6e7d81480dff4e8b3aa5262e528533fd8b9` /
`e74a8c0ecc7628ba2886e7ad10b4633d2e1ad0eac5ecf6c5ec86f057a9d1ab16`.
Warm work takes 0.541609/0.311504 seconds; complete lifecycles take
6.177062/3.647950 seconds. Manifest-owned compressed phases reproduce those
same 127 inputs in the production trace runner, and exact source/production
action-tick-facing-grounded equality is required at SHA-256
`dfa7be0339110c98c9107a069ef7e9751b14f2c174bd04a7e977c90ae745f6ad`.

The StandingTurn domain applies the same reusable route to a callback-scoped
state view. Pinned `9509dc04406fb2028bfab01243841ba4787c0fb7` and
repository-current `d882af94175e3c880ad51039e2979aa9a50aea09` have
byte-identical normalized `ftCo_Turn.c` at SHA-256
`3ad604c90ae3f67dd508cced55ab00ca6e7152a4a15693c5c78d4959434cbcfa`.
Before Turn's physical flip, its IASA temporarily exposes target facing to the
ordered special/catch/attack prefix and restores old facing before
guard/appeal/jump only when no earlier callback consumes. The manifest records
12 natural cases / 72 rows: jab, grab, three main-stick smashes, three tilts,
three C-stick smashes, and a post-restoration jump control. Both raw captures
hash to
`294e9eae84ae6bc92f5932e20d666479b4455e49bb9666a54052003ec94b2c59`;
source and production action-state/action-tick/facing/grounded payloads are
exactly equal at SHA-256
`0e9858e16140d8a55727255b009aec89e2176286e008b2cf23867bab38c2ac44`.
The generated immutable execution artifact hashes to
`a0f269dd6683e372629190bfa011f35f79be226fc6a2a150c49eebc55961b8a0`.
Production derives one stack-local effective facing and reuses the existing
special, grab, attack, and main-stick-smash routes; no persistent state,
allocation, table, parser field, duplicated frontend, or bespoke runner is
introduced.

This slice also demonstrates the required deterministic-replay repin audit.
The stale and rebuilt executables generate an identical 30,720-byte input
payload at SHA-256
`9f071030a71819ca234ee22a729d4fc0a371d61e941630d045fc35ed9a3d73a7`.
Canonical state checkpoints 0 through 217 and every earlier event are equal.
Zero-based input tick 217 then supplies slot 2 (human P3), already in
`STANDING_TURN` tick 2 facing -1 with turn direction +1, a fresh A edge and
main-stick X +20480. The stale executable enters Jab (`GROUND_ATTACK`, 12)
facing -1; the corrected executable enters forward tilt (`FORWARD_ATTACK`, 87)
facing +1. Completed-tick checkpoint 218 and its packed action-transition event
are the first divergences. The 240-tick / 42,519-byte result still terminates
with the same winner and 83 events, and its rebuilt corpus/final/event hashes
repeat on Windows and WSL as
`6727023fb07bcb7a4fcbaf9c0beac0f8220c1c1802b19da891ae2ae2be252240`,
`de96572115c1e4850d79353839576efc4b780ccbd75e8e70a2f23bee419c14af`, and
`124a94734029321020513ec749b2f4d26cd60b4ed2129e25ce104692739fa9af`.
Only after that first-divergence proof is it valid to repin the central fixture
and all active manifest, shell, and browser-smoke derivatives; matching final
digests alone would not authorize a golden update.

The reusable `native-csv-trace-v1` kind admits an already-qualified production
trace executable without adding another character-specific C adapter. Its
manifest owns runner arguments, compressed input phases, exact integer fields,
and half-open per-field row exclusions matching the live comparator. The
generator emits only immutable JSON execution metadata; the root verifier
expands inputs, runs independent cases concurrently, parses declared CSV
columns, restores manifest order, and hashes one canonical payload. Raptor
Boost uses this route for five cases / 502 samples, Falcon Kick for six cases /
399 samples, Run-to-RunBrake acquisition for four cases / 127 samples, and
StandingTurn callback-facing acquisition for 12 cases / 72 samples.
Raptor Boost's separate native Capsule
search remains live-only because the project-specific Relay Rod is not source-
equivalent item content.

Native CSV execution is centralized in `ssbm_native_csv_trace.py`; both the
fast stored gate and the live-source comparator consume the same canonical
runner result. Domains whose selected fields admit no representation tolerance
set `require_exact_source_match`. For those domains, the verifier requires the
production semantic digest to equal the qualified live-source digest before it
checks the pinned regression digest. This prevents a behavior divergence from
being hidden by updating a second independent golden. Nine acquisition
domains currently use this strict route: common special acquisition,
StandingTurn callback-facing acquisition, Run-to-RunBrake acquisition,
KneeBend up-special acquisition, crouch up-special acquisition, aerial
neutral-special turnaround, Teeter acquisition, GuardOff acquisition, and UCF
0.84 PAD/cardinal preprocessing.
Simultaneous semantic
edge actions and causal pre-edge input phases are declarative manifest data,
so state-specific callback priority and input-history boundaries need no
character-specific capture loop or native pre-roll.

The shared acquisition verifier also accepts an exact declarative edge-row
sequence plus an optional pre-edge action/frame. This keeps negative callback
cases strict when they remain in, or later leave, their source action instead
of forcing every case into one repeated acquired action. Character-overloaded
libmelee action names are resolved only by the domain manifest; they are not
added to the generic action map.

## Current pinned-UCF raw-input qualification

Modifier-sensitive evidence now targets the owner-supplied `GALE01` NTSC-U
revision-2 executable with official pinned UCF 0.84 enabled. The vanilla decomp
is still the base authority, but the oracle must also pin and reproduce the UCF
hook set and the raw controller history consumed at those boundaries.

The shared input path keeps the 32-byte frame fixed and transports signed raw
stick samples with per-axis validity alongside processed Q15 input. Missing raw
axes use the deterministic processed-input fallback. This permits exact live or
modern-replay evidence without falsely treating a legacy payload's absent raw
axis as zero. The character-independent Slippi differential parser/runner has
16 focused tests and requires exact disc/UCF provenance plus both raw main axes
before executing a candidate. The five tracked legacy files therefore remain
diagnostic: each lacks raw main Y and independent proof of the pinned UCF
revision.

The UCF common-special acquisition projection contains 22 cases / 272 rows.
Independent capture artifacts are byte-identical at
`0a3c853d039fb2b5552d195a040bbac5335aa1fe512f0260376f45c05d980027`.
Both canonicalize to the same source payload, which equals the native
production payload exactly at SHA-256
`087c81e3dbdc2794bc5bef1bbd8af32e68e3ee2fb36dc606b8ee266d5c1f2e4a`.
This directly live-qualifies the raw-history/Dashback boundary and delayed-Turn
primitives plus the Run-origin GuardOn CatchDash provenance and expiry split.

The separate UCF PAD/cardinal projection contains 16 cases / 48 rows for both
main and C sticks. The two independent capture SHA-256 values are
`d6d7cb26d0b30785bb38c39a6b400366742998d6f9f2eeb448f4a7cb31db4984`
and
`b46ef4c579a26050f6cb8f9eda6c6c5068dd5b62ac65eae4ccdc0d9847075372`;
their canonical source payloads and native production are exactly equal at
SHA-256
`4a553ba57522d4347188cb227357157fbb4f1a7246dd638fba68019e9166fd63`.
The current raw sample comes from Slippi's serialized main- and C-stick bytes;
DME supplies the source fighter's post-UCF processed axes. This split avoids
claiming that a delayed PAD queue read, the deterministic processed fallback,
or a browser adapter proves exact raw input. The two four-worker captures take
7.349 and 6.883 seconds; parent warm time is 8.097/7.357 seconds and cold time
is 9.728/8.722 seconds, under the domain's 12-second warm and 20-second cold
budgets. Direct verifier-only execution takes 232.529-251.415 ms on Windows
and 453.341-678.188 ms in WSL.

All eight UCF hooks were implemented and source-audited at this checkpoint.
PAD/cardinal was live-qualified alongside Dashback/raw-history; the six
then-open boundary families were closed by the later DBOOC/shield and sparse
damage-input decisions below.

The staged registry at this checkpoint declared 32 domains / 205 cases and passed with replay
under manifest SHA-256
`26b925f08337c64e8cb8db9c5de7e47488d92b2fc3a6dd887894f53cbd095647`.
Three isolated complete runs take 877.821-1,033.078 ms on Windows and
908.291-971.404 ms in WSL. The preceding
31-domain / 189-case registry's 240-tick replay is 42,555 bytes at
corpus/final/event SHA-256 values
`a1d9c1d97a3f20bdb9c76094c39b856f731a1eb2c0cca64ac05dd28a6e121949`,
`3bdbbbc5d7faa6c8fd077ebd47aaa061f738a3561aa4c66ae2bfe4f8455cda6a`,
and
`a4020969be032543b9b229c8801bde77581b9f7fe26a9fe8aca91527627b13ec`.
That preceding full Windows/WSL registry passes under manifest SHA-256
`34be35b31153031861cbe481cdd1d4e94dd158d079b09efee9561de3389e77aa`.
The verifier invokes the two character-independent generator APIs in-process
and caps its outer and nested worker pools, avoiding 31 redundant Python
startups without weakening generated-file checks. Three sequential Windows
runs take 943.548/903.249/1,068.009 ms and three WSL runs take
1,319.901/1,038.275/1,163.897 ms. Those measurements and the earlier
31-domain / 186-case numbers above remain completed historical checkpoints;
they are not relabeled as results for the expanded registry.

## Geometry-sampling limitation

ExiAI deliberately skips display-side bone evaluation while fast-forwarding.
That produces two observation-only differences which the verifier names and
constrains:

- Menu timing can enter the looping `STANDING` animation at a different phase.
  Only idle action cursor, hurtbox endpoints, and ECB are excluded. All actions
  that reset their cursor remain strict.
- `ftCo_8009388C` enters `GuardReflect` with `Ft_MF_SkipAnim`, so that motion
  state intentionally inherits the preceding pose instead of owning a
  GuardReflect animation sample. Its inherited endpoints and ECB are excluded;
  action, input, physics, shield behavior, and collision outcomes remain strict.
- Hurtbox endpoints on post-hit frames with positive hitlag can reflect the
  skipped display-side update. The collision result, damage, action, hitlag,
  and every other gameplay field remain strict, but those endpoints must never
  be imported as source geometry.

Process-local fighter addresses are also excluded. No other tolerance or field
exclusion is allowed. Geometry import routes must use an active, non-hitlag
sample; collision routes may use hitlag rows only for the already-completed
collision outcome and response state.

## Callback-owner replay repins require a causal boundary

Changing common animation-before-IASA ordering may legitimately alter a wide
replay tail, but equal Windows/WSL hashes alone are insufficient authorization
to repin. The callback-owner slice compared a pre-projection binary with the
new build and localized the first change: checkpoints 0 through 62 match, then
zero-based fixture input tick 62 processes terminal Forward-Air Landing while
full right is held. The new Wait callback enters Walk in that update; the old
model inserted Idle. Only after that source-backed boundary was established
were the active replay, verifier, registry, and browser derivatives updated.

The resulting 42,555-byte corpus/final/event identities are
`649b9ab2540b5e8d38b972756925b3349e82209235ed1aa8c58c8f51485ce1be`,
`e4834ffac8b7be8ce77cf604710ca307caea512cca5eb00fae8487ca0fdc75b4`,
and `787d63c5edf270cdc72d93dbe857c487bdc1ab7bdde59a1975299f1973fa7256`.
The browser re-simulation contains 82 typed events; the two-event reduction is
the removal of artificial intermediate action transitions, not lost evidence.

## Compose existing memory probes when a predicate crosses domains

UCF shield-drop qualification needs both controller-history memory and exact
pass-through surface identity on the same row. The checkpoint sharder therefore
supports a bounded `input-surface` composition of the existing probes. It emits
both ordinary descriptors, requires both as merge invariants, and is accepted
only for checkpoint special acquisition. This is not a new observation
protocol, and unrelated domains retain their prior single-probe schemas.

The reusable `platform_guard` setup runs natural Battlefield movement outside
the retained trace, then requires a trigger-valued guard phase and source floor
line 2 immediately before each tested edge. That gives one seven-case / 137-row
domain for DBOOC, shield-drop suppression, and extended pad count instead of a
surface-response fork or three duplicated runners. Its exact source/production
SHA-256 is
`73198f0ee5ab242d72598c4fa149d6f13e60112d69ddeb1d1f83e0218683c009`.

## Keep raw-history damage boundaries sparse and physical

UCF SDI, shield SDI, and tumble need two-frame controller history, but they do
not need a new oracle protocol. The live side reuses physical damage/shield
acquisition plus the schema-2 input-memory probe. The native side uses three
bounded physical pre-roll modes and then consumes the ordinary exact-raw CSV
rows. No runner writes damage, hitlag, tumble, ages, or history directly.

The retained theorem is intentionally small: four hitlag cases keep four rows
each, and two tumble cases keep three rows each. This is enough to prove strict
raw delta 62/63 and 75/76 behavior while excluding attack startup and long
DamageFall waiting from every routine stored-registry run. The dedicated live
verifier still validates the omitted physical acquisition, UCF runtime policy,
capture repeat, labels, raw samples, processed axes, ages, action, and result.

The resulting registry contains 35 domains / 218 cases under manifest SHA-256
`256cc0d55e882b5bff3a0dc52dc521db0d2e64ebb08ea9a479ee22ba81130946`.
Three full Windows runs take 619.867/679.644/788.424 ms; three WSL runs take
965.814/947.881/954.270 ms. The 2,000-ms gate still includes all 35
generated-file checks and deterministic replay. Earlier registry counts and
timings in this decision record remain historical snapshots.

## Preserve callback-entry provenance as compact canonical state

Two actions with the same public GuardOn state can own different callback move
variables. Run and late-Dash Guard entry call the source initializer that writes
PlCo `x68` to GuardOn `x24`; Wait Guard entry does not. Velocity, public action,
and current stick cannot reconstruct that provenance after entry, so the
smallest exact rollback contract is one bounded byte per player. It is imported
once, copied through the ordinary world/scratch/save path, consumed by the
shared GuardOn grab dispatcher, and cleared whenever the move-variable owner
changes.

The existing special-acquisition pack absorbed three cases rather than adding
a protocol or runner. Its 22 cases / 272 samples hash source and production
exactly to
`087c81e3dbdc2794bc5bef1bbd8af32e68e3ee2fb36dc606b8ee266d5c1f2e4a`.
The complete registry remains 35 domains and grows to 221 cases; three
sequential passes take 774.267-843.944 ms on Windows and 1,117.820-1,984.600 ms
in WSL under manifest
SHA-256 `9ef8f2ea5f5a21a0f8856dcc42fb8b5aa313de1f38cac31d8ab6f2a744180670`.

## Preserve same-action entry provenance in a compact phase

An action label is not always a complete oracle key. `ftCo_Dash` exposes the
same public Dash motion for ordinary and Turn-origin entry while x4 changes
the early callback table. Qualify paired routes on the same displayed frame
and preserve the smallest source move-variable bit in an existing action-local
phase when its range has room. For InitialDash, sign remains direction and
magnitudes 1/2 encode ordinary/Turn origin. This is allocation-free,
snapshot-stable, and avoids duplicate actions.

The existing special-acquisition protocol absorbed five cases: ordinary/Turn
early A, ordinary/Turn early shield, and ordinary early taunt. Its generic
input grammar gained Appeal, but no new protocol, probe, or runner was created.
Two 322-row captures canonicalize identically. The five provenance cases now
store both velocity channels and relative position, exposing and closing the
Dash-IASA x54 fallthrough plus target-state friction gap. The strengthened
29-case source/production digest is
`5e9a79db538b171c208737628a7667f97ffffc2915b46d4744876b30c7cfb51a`.
UCF/delayed Turn-to-Dash plus side-special and rejected neutral/up/down
fallthrough cases now use the same motion projection; only isolated one-Q16
samples are excluded.

The current registry contains 35 domains / 228 cases under manifest SHA-256
`d9abddfb6774379efde9081842ed0bd0f10924ea5d03008a841b1b9deab79055`.
Three isolated Windows runs take 1,103.784-1,343.801 ms and three WSL runs
take 1,187.017-1,371.182 ms. Earlier counts and timings above remain historical.

## Balance expanded checkpoint packs by retained rows

The common special-acquisition domain grew from 29 cases / 322 rows to 36
cases / 467 rows when Walk and Run gained their complete four-direction
special matrix. The first two-way 227/240-row partition produced valid output
but missed the 16-second warm wall gate at 17.010/17.035 seconds. A three-way
156/155/156-row partition reduced the critical worker without changing case
contents, labels, checkpoint slots, or canonical ordering.

Two accepted full captures take 12.410/14.011 seconds warm and
12.827/15.579 seconds cold. Their different raw artifact hashes canonicalize
to the same source/production SHA-256
`9546194d57f47eb320102b70be475111cead96c975e74af486201f3bf6d67cbb`.
No persistent Dolphin process, new runner, protocol, or web probe was needed;
the optimization is only a manifest-declared balanced partition of independent
immutable checkpoint cases.

The current registry contains 35 domains / 235 cases under manifest SHA-256
`f8637c42927fe1df228d4225607cbadd9211eae7a60a075e11c60f92e869b938`.
Three isolated Windows runs take 919.758-1,031.223 ms and three WSL runs take
1,394.273-1,589.864 ms. Earlier counts and timings above remain historical.
