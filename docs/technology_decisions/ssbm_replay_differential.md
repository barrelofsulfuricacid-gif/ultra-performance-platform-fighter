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

The active replay projection uses binary32 position, velocity, damage, and
geometry fields. Historical candidate deltas below remain in their capture-time
integer-grid units; current comparisons use the profile's explicit
`*_tolerance_f32` values and never reconstruct the retired representation.

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

The worker also has a deliberately separate discovery path for newer public
replays. `--allow-unverified-reference` may execute a replay only when the
recording says `UCF` and the extractor proves the complete serialized raw
main pair; unknown disc/UCF provenance is retained in every segment as
`reference_authority=diagnostic-unverified-reference`. Explicit provenance
mismatches, malformed framing, and missing raw axes remain rejected. A current
ranked-replay sample from the MIT-licensed
`erickfm/melee-ranked-replays` dataset (dataset revision
`11142d4b86d423716fdd2e9ca565de9bafc9d37e`) had a 64-byte pre-frame payload;
one Battlefield Falcon anchor executed 47 frames and passed with zero
candidates, but it is not an equivalence result because the source disc and
exact UCF revision are not encoded by that replay. The ignored manifest and
profile used for that run live under `build/` and are not part of the
qualification registry.

The larger discovery sweep uses the pinned MIT-licensed
[melee-ranked-replays dataset](https://huggingface.co/datasets/erickfm/melee-ranked-replays)
revision `11142d4b86d423716fdd2e9ca565de9bafc9d37e`. Its Falcon archive has
782 files. An initial 300 were hash-pinned and run through the worker. The
bounded eight-worker parser completed in 224.764 seconds with 300 parsed
replays, 259 anchors found, 53 diagnostic comparisons, and 1,303 checked
semantic frames. It recorded 21 diagnostic passes, 3 UCF dashback-boundary
observations, and 32 deterministic diagnostic candidates. Since the corpus
does not prove the exact disc image or UCF revision, every one of those
comparisons remains `diagnostic-unverified-reference`; this pass is useful for
finding candidate prefixes, not for claiming exact SSBM equivalence. The
ignored report is `build/slippi-differential/ranked-300-report.json` and the
300-entry manifest is `build/slippi-differential/ranked-300-manifest.json`.

The complete follow-up hash-pins and executes all 782 downloaded Falcon files.
Its manifest SHA-256 is
`f42d437ee166b07502a7af8316aa2ee0b473ea487802b1327ae673171493b477`.
Eight parser workers completed in 777.694 seconds: 328 natural anchors were
found, 111 diagnostic comparisons executed, 2,475 semantic frames were
checked, 39 prefixes passed, 7 UCF dashback boundaries were exercised, and 72
deterministic candidates remained. The ignored report is
`build/slippi-differential/ranked-782-report.json`, SHA-256
`154ae335f86fa91aa6bbc5fe0bb0a6908d594efd44f0d220bcdba27e238a6e8f`.
All results retain `diagnostic-unverified-reference`; archive size does not
replace exact disc and modifier provenance.

### 2,093-replay expansion and modifier classification

The current discovery corpus combines two non-overlapping Falcon archives
from dataset revision
`11142d4b86d423716fdd2e9ca565de9bafc9d37e`: the historical 782-file shard
and a second 1,311-file shard. Their archive SHA-256 values are
`e7906939235c1841d8abb2e8eb160a7ed84b0d0da35407b5b053b85c5b5f5acb` and
`60bb7e5cae1e469bdf54d646b60be5a5a77d0942a6c3b6405ba1600b23b2fe87`.
The extracted corpus contains 2,093 unique files / 7.08 GB; its ignored
hash-pinned manifest hashes to
`0e7b2b0805de1b09999b1f3104fb483cb2cbfbf2083864bb2d0d43cda3b6cd62`.

The first full pass found 887 natural anchors and executed 740 diagnostic
prefixes / 13,441 semantic frames. It recorded 210 passing prefixes, 396
explicit unsupported source-modifier boundaries, and 134 deterministic
diagnostic candidates in 1,265.375 seconds. The public replay setup records
only the broad `UCF` family. A new fail-closed classifier therefore detects
raw cardinal inputs that should snap under pinned UCF 0.84 but whose recorded
processed axes do not; those prefixes stop as
`ucf084-cardinal-signature-mismatch` instead of becoming false simulation
candidates.

Source analysis of the dominant candidate family found that terminal
KneeBend changes to Jump in the animation callback before the new update's
input callback. Jump entry consequently consumes the preceding processed
horizontal stick, while the newly installed Jump IASA consumes the current
input later in the same update. Production now feeds its ground-jump entry
from canonical prior processed X. Two independent six-row live cases prove
the neutral-history and held-history branches. Their full 491-row common
special-acquisition pack hashes to
`c68d3bc9cd830283648f98b210502a471ca0724fc3b5197caea5b71aaa29a07b`
for source and
`dfdf53934818d7436fa02fc265e3febb129e97f34b1573d0b670cfc66075a6c6`
for production; the only difference is one allowed Q16 unit on held Jump
frame 2.

The second complete pass executed the same 740 prefixes across 15,511 frames,
raised passes to 254, and reduced deterministic candidates from 134 to 58 in
1,102.245 seconds. Its ignored report SHA-256 is
`8792b8f0685912030a72baf0482433836df603971fb183d4103465ca8e46c1ad`.
The largest remaining 16-prefix cluster exposed a separate decomp-owned split:
Turn-origin Dash has `mv.co.dash.x4 == 0` and may reverse through the ordinary
Dash input callback before the early ordinary-Dash window ends. Production
now retains the existing zero-cost origin bit in that reversal gate. A natural
12-row live case proves Turn frames 1-7, Dash frames 1-3, then immediate Turn
frames 1-2 on the reverse edge. A third full-corpus pass is the regression gate
for that broader diagnostic cluster.

All 2,093 public files remain discovery evidence only. Neither dataset size nor
repeatability supplies the missing exact disc image and UCF revision. Candidate
promotion still requires decomp/UCF-source attribution plus the pinned live
Dolphin and stored-native gates. Twenty focused Python tests cover the worker's
input packing, modifier classification, provenance rejection, and prefix
minimization.

Two independent ranked replays exposed one shared input/callback boundary.
Pinned `ftCo_KneeBend_Anim` changes to Jump before that update's IASA, so a
terminal KneeBend update can consume EscapeAir and collide into
LandingFallSpecial immediately. Slippi pre-frame input also showed analog L at
its endpoint on the preceding frame and the physical L bit rising only on the
terminal update. The compact production input convention reserves
`UINT16_MAX` for the digital click; the replay adapter now caps an unclicked
analog endpoint at `65534` and emits `65535` only for the physical L/R bit.
Production factors one ground-jump entry and projects terminal JumpSquat to its
new airborne callback owner before IASA. The minimized replay advances through
the wavedash after this correction; its next mismatch is a later 96-Q16 aerial
horizontal-velocity residual, which remains a separate candidate.

The exact contract is independently exercised by the raw UCF boundary
`(processed X, raw X) = (0,0), (-0.5,-40), (-0.95,-76), (0,0)`: the production
CSV runner reports `Standing -> Turn(frame 1) -> Dash(frame 1) -> Dash(frame 2)`
with the UCF-facing flip. Eighteen focused Python tests cover exact/unknown
reference classification, missing raw Y, the strict 75/76 delta edge, mirrored
raw X, mask `3`, and deterministic prefix minimization.

## Local use

```powershell
python tools/ssbm_replay_differential.py bootstrap
python tools/ssbm_replay_differential.py run `
  --runner build/windows-msvc-release/movement_trace.exe
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
without Dolphin. The tracked implementation now exposes `watch`, which reruns
only when the hash-pinned manifest changes, so a long-lived discovery process
does not repeatedly launch the native runner while the corpus is unchanged:

```powershell
python tools/ssbm_replay_differential.py watch `
  --runner build/windows-msvc-release/movement_trace.exe `
  --interval-seconds 30 `
  --extract-workers 8
```

For discovery-only corpora, add `--allow-unverified-reference`; the report and
exit status still distinguish those runs from exact-reference candidates.
Confirmed candidates then enter the existing fast checkpoint live-oracle lane
after its active gameplay codes are provenance-checked. This division makes
broad discovery inexpensive while keeping the pinned disc, code
configuration, decomp, and identical-input live capture as the authority.
