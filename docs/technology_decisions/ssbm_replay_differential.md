# Bounded Slippi replay differential worker

Status: diagnostic offline lane, not an equivalence theorem or production
dependency.

## Decision

Use finalized Slippi pre-frame records as an input corpus and finalized
post-frame records as source observations. Start only from a naturally reached,
stationary `Wait` interval with neutral input history, replay the following
inputs through the production native CSV runner, and stop at the first boundary
outside a character profile's explicitly supported domain. The earliest
semantic difference is also the shortest time prefix that reproduces it.

Production comparison is now gated on the exact project reference: the
owner-verified GALE01 NTSC-U revision 2 disc, pinned UCF 0.84, and a Slippi
pre-frame payload that physically contains both signed raw main-stick bytes.
The normalized float pair and those exact raw bytes travel together through
the runner's 12-field input row with validity mask `3`. An audited UCF
Turn-to-Dash boundary is therefore ordinary compared behavior when all three
authorities are present; missing raw bytes or unknown disc/UCF provenance stop
the replay before it can produce a pass or simulation-gap claim.

This adds no simulation code, production allocation, or runtime branch. Parser,
downloads, reports, and runner processes live under ignored `build/` paths.

The checked-in parts are character-independent worker code plus data profiles:

- `tools/ssbm_replay_differential.py`: provenance, qualification, comparison,
  modifier classification, and prefix minimization.
- `tools/ssbm_slippi_extract.mjs`: finalized Slippi field extraction plus a
  fail-closed walk of the file-declared event sizes for raw main X/Y bytes.
- `tools/ssbm_falcon_replay_profile.json`: Falcon action/scale/domain mapping.
- `tools/ssbm_falcon_replay_corpus.json`: five pinned CC0 replay artifacts.

## Prior-art sweep

The sweep was performed before implementing the worker:

- [project-slippi/slippi-js](https://github.com/project-slippi/slippi-js) at
  `626f8fb0dfa08133f3244d58bebe7002bddd44bf`; the worker pins published package
  `@slippi/slippi-js@9.1.2` (LGPL-3.0-or-later). It is the maintained official
  parser and exposes setup, pre-frame, and post-frame fields. `getFrames()` is
  keyed by frame number, so the extractor consumes finalized rollback
  replacements rather than duplicate speculative frames.
- [hohav/peppi](https://github.com/hohav/peppi) v2.1.2 at
  `b3099d5f14f10b0c9cadf5edb20632e843adfe50` (MIT) is maintained and provides a
  Rust/Arrow route for much larger corpora. It is a good future throughput
  option, but adding a Rust/Arrow dependency is unnecessary for this five-file
  bounded lane.
- [project-slippi/slippi-ssbm-asm](https://github.com/project-slippi/slippi-ssbm-asm)
  at `fcf47f10dc244152c2ebaa3a9dec142ea42243b7` documents actual recording and
  playback behavior. Playback restores processed controls and raw controller
  history. Optional resync code also overwrites state such as position, facing,
  action, percent, and RNG, so a resynced playback cannot serve as independent
  evidence that production remained equivalent.
- At that pinned recorder revision, `SendGamePreFrame.asm` stores raw main X at
  event offset `0x3b`, raw main Y at `0x40`, and both raw C-stick bytes at
  `0x41`/`0x42`; the declared pre-frame payload is 66 bytes. The pinned
  `slippi-js` public type still exposes only `rawJoystickX`, so the extractor
  reads the extra bytes from the file's own `MESSAGE_SIZES` framing and
  cross-checks raw X against `slippi-js`. It never invents raw Y for older
  payloads.
- The repository's ExiAI/checkpoint/live-trace lane is the pinned-disc mechanism
  for targeted proof, but it is not presently a vanilla oracle by configuration:
  the libmelee/Slippi `Required General Codes` setup enables UCF 0.84. That
  configuration is now deliberately sanitized to the project's pinned UCF
  0.84 gameplay-only policy. A live result must carry that policy provenance.
  The internal PF replay/hash corpus proves target determinism, not source
  equivalence. The native CSV runner is reused here so the offline worker
  exercises production rather than a second model.
- The public seed corpus is from
  [erickfm/slippi-public-dataset-v3.7](https://huggingface.co/datasets/erickfm/slippi-public-dataset-v3.7)
  at revision `6b995d97f3120111e2adaaf88a131deb4f06b41e` under CC0-1.0. Every selected
  object is pinned by path, byte size, and SHA-256.

## Architecture and evidence boundary

```text
pinned manifest -> hash-verified .slp -> finalized pre/post frames
                                        |
                           stable neutral Wait anchor
                                        |
                         profile-bounded source prefix
                                        |
                  identical ordered inputs -> production runner
                                        |
          strict state + explicit Q16 tolerances -> first difference
                                        |
                      shortest temporal prefix + repeat run
```

The worker first verifies a manifest-owned exact disc/UCF source declaration,
the replay's observed UCF controller-fix family, and raw payload layout. It then
compares mapped action, facing, grounded state, and relative X/Y.
Self-induced velocity is compared only when the replay version serializes it.
Tolerance values are profile data and are always emitted in the report; the
worker does not widen them after observing a result.

The report keeps setup provenance, controller-fix declarations, fields missing
from old replay formats, qualification stop reasons, source and production rows,
the first mismatch, and deterministic reruns of its minimal temporal prefix.
A result is a candidate to qualify with the pinned-disc live oracle and decomp;
it is never silently promoted to whole-game equivalence.

## Explicit unsupported cases

- Arbitrary mid-match state restoration. Only a naturally stationary `Wait`
  anchor with eight neutral frames is admitted.
- PAL, teams, unsupported stages, non-profile fighters, platform contact,
  opponent proximity, damage/stock changes, and unmapped actions.
- Missing external proof of the exact disc revision. `.slp` setup does not
  encode a disc SHA-256, so a corpus entry without independent provenance is
  `unsupported-reference-configuration`.
- Missing external proof of exact UCF 0.84. Old setup records say only `UCF`;
  that label is necessary but not sufficient.
- Missing exact raw main X/Y. Historical 63-byte pre-frame payloads contain raw
  X at `0x3b` but end after percent at `0x3f`; they cannot be upgraded by
  reverse-quantizing normalized floats. Exact modern pairs use mask `3`.
- Malformed or unknown PAL/team/stage/player setup and an unrepresentable
  mirrored signed raw `-128` sample.
- Fields absent from old Slippi versions, including self-induced velocity,
  hitlag, and animation index. Their absence is reported rather than replaced
  with guessed values.

The tracked five-file CC0 corpus remains useful provenance/anchor inventory,
but all five files are Slippi 2.0.1 with 63-byte pre-frame payloads. A bounded
run found the same nine natural anchors, then rejected all five with exact
reasons: disc identity unproven, UCF revision unproven, and raw main Y absent.
No prior diagnostic candidate was silently promoted or reclassified as a
production gap.

The exact contract is independently exercised by the raw UCF boundary
`(processed X, raw X) = (0,0), (-0.5,-40), (-0.95,-76), (0,0)`: the production
CSV runner reports `Standing -> Turn(frame 1) -> Dash(frame 1) -> Dash(frame 2)`
with the UCF-facing flip. Fifteen focused Python tests cover exact/unknown
reference classification, missing raw Y, the strict 75/76 delta edge, mirrored
raw X, mask `3`, and deterministic prefix minimization.

## Local use

```powershell
python tools/ssbm_replay_differential.py bootstrap
python tools/ssbm_replay_differential.py run `
  --runner build/windows-msvc-release/pf_m4_movement_trace.exe
python -m unittest tests/tools/test_ssbm_replay_differential.py
```

`run` exits 1 when it records one or more exact-reference semantic differential
candidates, 0 when every exact segment passes or inputs are explicitly rejected
by the setup/reference gate, and 2 for malformed configuration/provenance/tool
failures. The complete
machine-readable report defaults to ignored
`build/ssbm-replay-differential/report.json`.

## Ongoing worker and CI shape

Keep the corpus out of required pull-request CI. A separate scheduled and
`workflow_dispatch` job can cache the parser and corpus by manifest digest,
shard a larger legal replay manifest, upload JSON reports/minimized prefixes,
and alert only on new candidates. Normal CI may run the small pure-Python
classifier/minimizer unit test; it should not download replays or launch
Dolphin.

A continuously running local worker can process new pinned manifest entries
without Dolphin. Confirmed candidates then enter the existing fast checkpoint
live-oracle lane after its active gameplay codes are provenance-checked. This
division makes broad discovery inexpensive while keeping the pinned disc, code
configuration, decomp, and identical-input live capture as the authority.
