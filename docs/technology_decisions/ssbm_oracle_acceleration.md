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

A project-authored Slippi 3.5.1 NoGUI/unthrottled build was prototyped before
the broader sweep. WSL lacked direct render-device access, its software-OpenGL
601-row run took 38.15 seconds, and maintaining that fork duplicated ExiAI.
It is rejected as the default. A native binary patch is likewise rejected in
favor of the published, source-available release.

The sweep found no supported ExiAI/libmelee save-state experiment protocol.
The maintained reusable mechanisms are fast-forward, the long-lived ENet
helper, and instant match restart. A custom persistent/save-state layer should
be considered only if profiling shows meaningful time remains after traces
with the same configuration are batched.

## Reproducible setup

From WSL at the repository root:

```sh
tools/bootstrap_ssbm_exiai_oracle.sh
```

The bootstrap downloads and verifies the release artifact, extracts it once,
creates an ignored Python environment, pins `melee==0.47.2` and
`dolphin-memory-engine==1.3.1`, and validates the launcher identity. It never
downloads, copies, or modifies the game image.

An accelerated capture uses the generated paths:

```sh
build/oracle-toolchain/exiai-python/bin/python \
  tools/capture_ssbm_movement.py \
  --dolphin build/oracle-toolchain/exiai-0.2.0/squashfs-root/AppRun \
  --oracle-release-artifact \
    build/oracle-toolchain/exiai-0.2.0/Slippi_Online-x86_64-ExiAI.AppImage \
  --iso /owner/path/to/GALE01.iso \
  --output build/oracle/capture-exiai.json \
  --common-hurt-geometry-only --memory-probe-hitbox --oracle-exiai
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

The current qualification trace contains 650 rows and three Falcon common-hurt routes:
Initial Dash, CrouchStart/CrouchEnd, and KneeBend, including physical hit/miss
controls. The automated A/B comparison strictly matches every requested and
observed input, game frame, active action and action frame, position, velocity,
damage, hitlag, collision decision, and active non-hitlag geometry observation.
It covers 260 non-standing Falcon rows and 194 non-standing opponent rows.
Of those, 251 Falcon and 186 opponent rows are non-hitlag active-pose samples
whose complete capsule geometry also matches exactly.

Measured on the local WSL host:

| Runner | Wall time | Result |
|---|---:|---|
| stock Windows Slippi 3.5.1 / libmelee 0.40.1, 601 rows | about 28.3 s | baseline |
| project-authored WSL NoGUI/software OpenGL, 601 rows | 38.15 s | rejected |
| ExiAI unaccelerated control, headless, 650 rows | 22.17 s | qualification control |
| ExiAI headless/null/fast-forward, 650 rows | 16.68 s | selected |

The memory probe dominates this geometry-heavy trace, so fast-forward's
measured benefit is smaller than in ordinary state-only traces. The selected
path still runs a larger capture about 41% faster than the previous stock runner,
removes GUI launches, and supplies frames as quickly as the blocking observer
can consume them. Two upstream live tests, each launching its own match, took
7.92 seconds total on the same host.

## Geometry-sampling limitation

ExiAI deliberately skips display-side bone evaluation while fast-forwarding.
That produces two observation-only differences which the verifier names and
constrains:

- Menu timing can enter the looping `STANDING` animation at a different phase.
  Only idle hurtbox endpoints/ECB and the non-participating opponent's idle
  cursor are excluded. All actions that reset their cursor remain strict.
- Hurtbox endpoints on post-hit frames with positive hitlag can reflect the
  skipped display-side update. The collision result, damage, action, hitlag,
  and every other gameplay field remain strict, but those endpoints must never
  be imported as source geometry.

Process-local fighter addresses are also excluded. No other tolerance or field
exclusion is allowed. Geometry import routes must use an active, non-hitlag
sample; collision routes may use hitlag rows only for the already-completed
collision outcome and response state.
