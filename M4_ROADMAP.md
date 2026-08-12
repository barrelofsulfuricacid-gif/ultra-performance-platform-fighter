# M4 execution roadmap

Last updated: 2026-08-12

This is the short, current status companion to
`ultra_performance_platform_fighter_implementation_plan.md`. A row is marked
done only when its implementation and required local verification are both
complete. Live Dolphin qualification and stored-oracle regression are tracked
separately because a stored pass cannot establish new SSBM truth.

## Goal

1. Make the SSBM-to-simulation equivalence harness fast, reusable across
   characters, and explicit about the behavior domains it proves.
2. Finish the Captain Falcon NTSC 1.02 port with official pinned UCF 0.84
   enabled, allowing only documented bounded Q16.16 representation
   differences.
3. Deliver a native playtest frontend with Battlefield.
4. Continuously improve the `ssbm-character-importer` skill so later character
   ports reuse common import, oracle, and runtime machinery.

## Current snapshot

| Workstream | State | Current evidence or next gate |
| --- | --- | --- |
| Reference ruleset | pinned UCF target explicit | The behavioral target is the owner-supplied `GALE01` NTSC-U revision 2 executable with official pinned UCF 0.84 enabled. Vanilla decomp remains the base authority, but modifier-sensitive claims also require UCF source/hook provenance and the raw controller history consumed by that build. |
| Fast stored equivalence | thirty-five-domain UCF/raw-input registry; every Falcon-relevant UCF hook family has a direct boundary domain | The registry now contains 35 domains / 235 cases under manifest SHA-256 `f8637c42927fe1df228d4225607cbadd9211eae7a60a075e11c60f92e869b938`; three isolated complete runs take 919.758-1,031.223 ms on Windows and 1,394.273-1,589.864 ms in WSL. The common special-acquisition pack owns 36 cases / 467 samples and adds all four Walk and Run special directions to the existing exact callback-order, UCF/vanilla reversal, InitialDash provenance, and GuardOn-origin matrix. Earlier registry counts and measurements later in this file remain historical checkpoints. The current 240-tick replay is 42,559 bytes at corpus/final/event SHA-256 `fb0f4e7251e70f7660801222b5b5a2627e9c45e1b56b7d5763035947cb553d1c` / `5a7db4a5e899b1af31909f7997dcb1a08226aec79f4f09fab7422fe9602f246f` / `787d63c5edf270cdc72d93dbe857c487bdc1ab7bdde59a1975299f1973fa7256`. |
| Fast live Dolphin oracle | all eight Falcon-relevant pinned-UCF hook families directly live-qualified | Registered packs continue to use headless/null/unlimited ExiAI and checkpoint-isolated cases. Dashback/raw history, PAD/cardinal preprocessing, DBOOC, Shield Drop, and Extended Shield Drop retain their exact domains. The new damage-input slice adds four 4-row hitlag cases at the strict raw-delta 62/63 SDI and shield-SDI boundaries, plus two 3-row DamageFall cases at the strict 75/76 tumble boundary. Hitlag source/production hashes are `9f30698ba7ec1aafc5dd1bbb15e1a6f8bc1f503d04a4e86a318e74a5be3a87e4` / `7754c6342a567433d4fb4989405c9e309429782aa52dc34e49f76133b0f01303`; the only allowance is a measured one-Q16 position delta. Tumble is byte-exact at `da5473c7bfd0883a405eef293d11eca8f7618f78999e42f73097aff99760ff00`. The Zelda grounded-Up-B cardinal exception remains outside this Falcon domain and must be qualified when Zelda is imported. |
| Falcon bounded hurt poses | common, complete Wait lifecycle, guard, turn, ledge, ordinary airborne, pummel/capture, crouch-wait, taunt, dynamic ground-loop, GuardSetOff, shield-break, all 26 ordinary-action ECB routes, and all 17 common damage motions imported | Base Wait's natural SquatRv entry and six-update blend remain qualified within one Q16 unit. Two byte-identical 440-row lifecycle captures now add every Wait2/Wait3 frame, exact 70/20/10 weighted HSD-RNG selection, same-secondary rejection, zero-blend variant entry, and six-update return/restart blends. The shared HSD evaluator matches 145 direct variant poses / 1,595 capsules plus ECB within one Q16 unit. Its action-owned extension matches 2,086 paired live observations / 1,850 unique frames for all three jabs, every rapid-jab phase, Dash Attack, all five forward tilts, Up/Down Tilt, three real forward-smash angles, Up/Down Smash, both grabs, and all five aerial attacks at maximum one-Q16 error for every qualified component. DamageN2 adds two independent 72-row physical captures: 276 qualified pose observations / 3,036 capsules and every post-entry damage-owned ECB agree within one Q16 unit. DamageFlyRoll additionally applies the decomp's velocity-oriented `XRotN` override to both hurt and ECB evaluation without a separate pose table. Existing guard, Turn/TurnRun, ledge, airborne, pummel/capture, grounded-loop, GuardSetOff, and shield-break qualifications remain intact. |
| Falcon movement and combat | partial | Captured routes include wall/ceiling response, flat-floor missed/neutral/directional techs, both Up/Down prone/getup orientations, grounded player push from both ports/directions, imported Hyrule slope/DownBound/ordinary-ledge response, the exact common-data `x480` down-input ledge rejection boundary, all eight quick/slow ledge options, exact 640/480-frame CliffWait timeout and regrab cooldown, ordinary Jump/Fall airborne animation clocks, Falcon Punch's complete ground/air clocks and qualified air-physics tail, Raptor Boost's five complete fighter routes plus its live-only native Capsule branch, all six Falcon Kick routes, and a source-qualified complete Battlefield collision/environment catalog. Production imports complete Jump/Fall and all five aerial-attack HSD tracks, all 198 ShieldBreakFly/DownD/StandD/Furafura frames, all 25 GuardOn/Guard/GuardOff poses, both 26-frame DownBound tracks, both 70-frame DownWait loops, both neutral getups, both getup attacks, all four prone-orientation/direction roll tracks, and the complete 158-frame CrouchWait ECB cycle. Common acquisition is callback-order exact for SquatWait, SquatRv, Turn, KneeBend, Ottotto, OttottoWait, ordinary air, released Damage/DamageFall, powershield-enabled GuardOff, and the captured Run/RunBrake/locked-TurnRun down-input split. Direct EscapeN is now limited to Wait and callbacks that actually delegate to Wait: fresh shield+down from Walk or crouch enters GuardOn first, while Wait enters SpotDodge immediately. Walk now follows its exact movement tail: a fresh full opposite input is consumed by Dash and enters smash Turn, while a sub-dash opposite tilt falls through to `ft_8008A244` and Wait without a synthetic basic Turn. Run and late-Dash Guard entry retain the imported three-update GuardOn `x24` provenance: fresh A during that window enters dash grab, Wait-origin GuardOn enters ordinary grab, and an expired Run-origin window returns to ordinary grab. InitialDash now also retains source entry provenance at zero state-size cost: ordinary entry maps early A to F-smash and held shield to forward roll through imported PlCo `x48=3`, while Turn-origin entry maps the same displayed Dash frame to Dash Attack and Guard; Taunt is available from every Dash phase. The motion-strengthened 36-case / 467-row source and production pack is byte-exact at `9546194d57f47eb320102b70be475111cead96c975e74af486201f3bf6d67cbb`; it adds exact four-direction Walk/Run special acquisition to the existing UCF/vanilla reversal, InitialDash, and GuardOn provenance matrix. Early basic Turn now exposes its pending facing to the source special/grab/attack prefix, including main-stick and C-stick smashes, while the rejected-prefix jump control keeps the old facing; all 12 live/native cases hash exactly to `0e9858e16140d8a55727255b009aec89e2176286e008b2cf23867bab38c2ac44`. Full down from ordinary Run enters RunBrake rather than crouch; radial-gate diagonal down continues Run for the edge row, down from RunBrake enters crouch, and the locked Run phase after TurnRun rejects direct down. Released non-tumble Damage delegates to ordinary airborne IASA and accepts fresh digital trigger into AirDodge; released tumbling DamageFly/DamageFall omits EscapeAir and rejects the same edge. DamageFall also follows its dedicated callback table: ordinary A/C-stick aerial attacks are ignored while special and usable aerial jump retain priority, and a qualifying ordinary/UCF wiggle cannot be overwritten by a same-frame aerial. The remaining acquisition coverage includes KneeBend's up-special-only dispatcher before grab/up-smash, diagonal-down arbitration, Turn's missing neutral-B callback, aerial neutral-B's strict recent-opposite turnaround window, all four teeter specials, and GuardOff's complete special/attack/grab branch plus its shared jump/spot-dodge tail and ordinary negative controls. The shared common-air conversion owns the decomp's ten-update previous-bottom ECB lock; aerial attacks no longer use a route-captured bottom table. All 26 ordinary action routes derive collision ECBs from the allocation-free evaluator, with 925 runtime poses covered by the native primitive. A generated submotion flag selects Melee's TransN-stripped or model-root ECB reference space without action-specific branches. A fresh 165-frame natural Falcon Dive miss route passes the identical-input comparator with the documented 640-Q16 position envelope. FallSpecial additionally reproduces the decomp's velocity-selected neutral/forward/back target, switch-update double blend, stable-update single blend, and persistent bottom-lock state. The former Raptor/Falcon Dive/FallSpecial/aerial-attack authored ECB arrays are removed. Wider callback/state coverage stays open. |
| Common damage response | numeric response and all common damage-motion selectors implemented; represented routes qualified | The six-case live Dolphin trace and generic stored numeric trace pass. Production ports the decomp's pre-launch ground/air, knockback-level, collided-hurtbox-height, strict 70/110-degree DamageFlyTop cone, 100% DamageFlyRoll threshold, and exact HSD-RNG draw. It also imports the slope launch/projection rule, level-three steep-floor reflection, ground-origin ECB lock, landing-entry channel ownership, and the non-tumble floor selector's exact 5.0/0.5 source-speed thresholds. A deterministic four-case/120-row Hyrule line-36 theorem covers both Forward-Tilt launch branches plus actual low-speed jabs that land below 0.5, retain DamageN2 after hitstun release, and either exit on the sourced terminal animation frame or accept Falcon Punch through the released common Wait IASA callback. A separate two-case/68-row theorem closes the airborne release split: ordinary non-tumble Damage accepts fresh L into AirDodge, while DamageFly/DamageFall rejects it and remains tumbling. Its two-case/four-sample stored domain hashes production at `cec3d2b1d9b67ad906bf68b074c8975f6e53bf48cc714650c6498bef7aeba93e`. DamageN2 live pose/ECB evidence includes all six mixed hit-entry rows. Broader attack/stage routes remain open. |
| Separate knockback velocity and decay | open-air and flat-ground routes qualified | A pinned 64-row late DashAttack route agrees for 15 damage samples on action/frame, grounded/tumble, damage, timers, self velocity, projected knockback, and `xF0_ground_kb_vel` within 0.001 source units. Canonical save/load, replay, Windows, WSL, and sanitizers pass. |
| Remaining Falcon gaps | not complete | Work through every incomplete row in `docs/product/m4_ssbm_fidelity_audit.md`; do not infer whole-character equivalence from one domain. |
| Native Battlefield frontend | implemented locally; hands-on gate remains | The SDL target runs the real simulation at fixed 60 Hz, supports 2P Duel and 4P Teams plus 1-8 stocks through the core config contract, renders the complete source-derived 23-line Battlefield catalog and blast-zone inset, and visualizes fighters, crouch, shields, hitboxes, exact 11-capsule source hurt poses, damage, stocks, actions, and a terminal winner/time-limit banner with an `R - REMATCH` cue. SDL's GameCube type now follows its pinned A/X/B/Y positional contract: A attacks, B is special, X/Y jump, Z uses the production grab chord, Start taunts, and analog/digital L/R remain shields. The explicit Mayflash `0079:1843` raw PC path is unchanged. Strict MSVC, WSL, mapping smoke, and existing Battlefield screenshot QA pass. Real-controller input plus completed-result visual confirmation remain in one hands-on pass. |
| Web acceptance architecture | duplicate probes removed; current replay smoke passes | Four source-grep verifier scripts and duplicated gameplay scenarios are deleted. The compact `web.m4_playtest` gate protects only the real bridge ABI: exact main/C-stick/button/independent-trigger forwarding, Team Lab slot routing, and dynamic shield/hit-sphere packing. A fresh Emscripten build and real headless Chrome run pass the 42,559-byte ABI-5 replay with final SHA-256 `5a7db4a5e899b1af31909f7997dcb1a08226aec79f4f09fab7422fe9602f246f`. Collision-toggle, result/rematch, and replay-navigation/fail-closed import remain honestly named owner-interaction gates instead of being overclaimed from a static DOM dump. |
| Slippi replay differential | exact UCF input path implemented; tracked legacy corpus fail-closed; 300-replay diagnostic sweep and continuous watcher added | The character-independent worker and five-file CC0 Falcon corpus remain diagnostic discovery infrastructure. The extractor now walks the file-declared Slippi event framing to retain physically serialized raw main X/Y and cross-checks raw X against pinned `slippi-js`; an exact processed/raw pair reaches the production CSV runner through its 12-field row with validity mask `3`. The former unsupported-UCF-raw-history suppression is removed only after a corpus entry proves the GALE01 revision-2 disc, pinned UCF 0.84, observed UCF controller-fix family, and both raw main axes. A production probe using raw X `0,-40,-76,0` now reaches `Standing -> Turn 1 -> Dash 1 -> Dash 2`, exercising the 75/76 UCF dashback edge, and 17 focused Python tests pass. A fresh bounded run hash-verifies and parses all five tracked files and rediscovers nine anchors, but executes zero: every legacy Slippi 2.0.1 file has a 63-byte pre-frame payload (exact raw X, no raw Y) and lacks independent disc/UCF-revision proof. The report classifies all five as `unsupported-reference-configuration`; the earlier five movement candidates remain diagnostic history and are not promoted, dismissed, or counted as current exact-reference gaps. A separately labeled `--allow-unverified-reference` path ran one modern 64-byte-payload Falcon replay for one 47-frame Battlefield anchor with zero candidates; it is explicitly not equivalence evidence. The pinned MIT-licensed ranked-replay archive contains 782 Falcon files; a SHA-256 manifest of its first 300 entries (`9e13187aa364de9315414fc4c56a7e99e3521b3ed100c618d7f32a0d96600f9a`) ran with eight bounded parser workers in 224.764 seconds: 300 parsed, 259 anchors found, 53 diagnostic anchors executed across 1,303 frames, 21 diagnostic passes, 3 UCF dashback boundaries, and 32 diagnostic candidates. All 300 still lack independently proven disc/UCF revision, so none of those passes/candidates are exact-equivalence claims; the machine report is ignored under `build/slippi-differential/ranked-300-report.json` (SHA-256 `3f1b62cd3be996b51234669b8684669b3146a58bd7c5fb5912e891bf68cd99bc`). `watch` reruns only when the hash-pinned manifest changes, preventing idle native-process churn. The core now also projects terminal strong AttackAir ownership to Fall before IASA; a focused movement regression confirms terminal EscapeAir. |
| Character-importer skill | active | The skill records reusable HSD/PlCo import, damage-channel, callback-order, ground-projection, save/load, action-release, physical surface-route, lifecycle, semantic-digest, `StageInfo`/JObj stage-import, per-surface collision routing, previous/current animated-ECB identity, bounded one-way-platform crossing qualification, source-pose matrix-branch guidance, moving-target recurrence, compact source-replay descriptors for wide SRT transitions, target-skeleton switch versus stable-update blend semantics, `Ft_MF_SkipAnim` manual/frozen guard-pose clocks, clean native Wait entry, blend-completion boundaries, HSD-RNG-owned idle variants, action-owned displayed-frame/HSD-frame offsets, callback-phase delegation, natural controller-driven endpoint fixtures, symmetric two-fighter collision-history resets, opponent-owned semantic input edges, and independent live qualification of HSD hurt poses versus runtime-derived ECBs. |

The complete frame-data generator is source-closed against the current
qualified shield-break profile: two regenerations produce include SHA-256
`e936f0edef8cdab44a6507d8b1c7f5474ea1950ead5a82a1f5f9d2a2e9478ebe`.
The active implementation slice continues the decomp/runtime fidelity audit
after closing Run-to-RunBrake acquisition, DamageFlyTop/Roll selection,
mixed hit-entry ECB ownership, grounded slope launch/recontact, the non-tumble
floor-response selector, retained Damage lifecycle, the released Damage versus
DamageFall air-dodge callback split, and common-state special-acquisition masks,
including both Ottotto phases. Numeric response remains qualified separately.

## Completed and live-qualified: Walk/Run four-direction special acquisition

- [x] Audit the pinned `ftCo_Walk_IASA` and `ftCo_Run_IASA` bodies rather than
  trusting helper names. Both own Side, Up, Neutral, and Down Special; Walk
  checks Catch first, while Run checks all four specials before CatchDash.
- [x] Extend the existing checkpoint/native-CSV domain with seven cases: the
  three missing Walk directions and all four Run directions. No new protocol,
  native runner, gameplay branch, canonical byte, or web probe was added.
- [x] Pin two independent 467-row captures at raw SHA-256
  `34f721aea3df67edaea7435a256cc70e27d54767a2ae9b61fe4f6e22bc749ceb`
  and `d9af7fbc86479d1e3a8f823ae77e3bf5f8510d98db0c27ddeaa774436897033f`.
  Their semantic trace and Windows/WSL production are byte-exact at
  `9546194d57f47eb320102b70be475111cead96c975e74af486201f3bf6d67cbb`.
- [x] Balance the pack into three 156/155/156-row workers. The two final runs
  take 12.410-14.011 seconds warm and 12.827-15.579 seconds cold, within the
  existing 16/20-second budgets. The generated oracle hashes to
  `23e7a23d300585b93ffe7d877ac4273ab1e00ece4bf160b57ba660c7b5df618e`.

## Completed and live-qualified: InitialDash entry provenance

- [x] Preserve `ftCo_Dash` move-variable `x4` in the existing signed
  `dash_direction` byte: sign is direction, magnitude 1 is ordinary entry,
  and magnitude 2 is Turn-origin entry. This adds no canonical byte, snapshot
  field, replay field, allocation, or duplicate action.
- [x] Import PlCo `x48=3` and reproduce the early callback split. Ordinary
  Dash frame 2 accepts A as F-smash and held shield as forward roll;
  Turn-origin Dash frame 2 accepts A as Dash Attack and held shield as Guard.
  Taunt is accepted from ordinary early Dash as required by source `block_42`.
- [x] Import PlCo `x54=0.75` and preserve successful Guard/Appeal IASA
  fallthrough before the target-state physics callback. One shared
  reference-aware stationary-ground-friction helper serves Guard,
  GuardOn/Off/SetOff, and Appeal without changing authored fighters.
- [x] Pin independent 322-row captures with raw SHA-256
  `750800f604c03baed6c74870b43b624957cb65a89a8570c1f905b608eb1021c3`
  and `fb59fd9809c4f805436cfa0e298e9b4b452b562e6a489fb542147393d112cb15`.
  The widened 29-case source and production traces are byte-exact at
  `5e9a79db538b171c208737628a7667f97ffffc2915b46d4744876b30c7cfb51a`
  across action, facing, grounding, velocity, and selected relative position.
- [x] Regenerate the strengthened stored oracle (SHA-256
  `3fee9dc65808095e2014a1c42c1765176f6cfcc8cadd3f0ce7e2f6868a9a5b0b`)
  and expand the full registry to 35 domains / 228 cases. The unchanged
  240-input replay keeps its final/event hashes; its corpus hash repins only
  because the content contract now includes imported PlCo `x48`.

## Completed and live-qualified: Walk reversal fallthrough

- [x] Preserve the exact `ftCo_Walk_IASA` tail: Dash is checked before
  Squat/Wait, and `ft_8008A244` enters Wait for every remaining
  opposite-facing stick. The source-only branch reuses the ordinary Wait
  friction path; authored/custom fighters retain their existing basic-Turn
  behavior.
- [x] Pin two independent 278-row headless Dolphin captures, both raw SHA-256
  `e4193dce5d782716f41d35b7495e94a345142412c0dbc4813aa430907a998a3a`.
  The new paired rows prove moderate opposite Walk -> Wait and fresh full
  reversal -> smash Turn with exact action, facing, grounding, and X-age.
- [x] Regenerate the 24-case stored pack. Source and Windows/WSL production are
  byte-exact at
  `5a22a6f401df8a8557bd2ac16b5c3dd34211cf825f302a9f81db2f4e2897253f`;
  the focused stored gate passes in 250.781 ms. Three complete registry runs
  take 887.531-946.419 ms on Windows and 1,075.190-1,131.290 ms in WSL.

## Completed and live-qualified: Run-origin GuardOn dash-grab provenance

- [x] Import PlCo common-data `x68` as the three-update GuardOn dash-grab
  window. Run and late InitialDash set the window on GuardOn entry; Wait and
  other owners clear it. The shared GuardOn grab dispatcher consumes the one
  canonical byte without duplicating Run/Dash callbacks.
- [x] Pin two deterministic 272-row headless Dolphin captures, both raw
  SHA-256 `0a3c853d039fb2b5552d195a040bbac5335aa1fe512f0260376f45c05d980027`.
  Their exact cases prove Run-origin `GRAB_RUNNING`, Wait-origin ordinary
  `GRAB`, and ordinary `GRAB` after four GuardOn rows exhaust the three-tick
  source window.
- [x] Regenerate the 22-case stored pack. Source and Windows/WSL production
  are byte-exact at
  `087c81e3dbdc2794bc5bef1bbd8af32e68e3ee2fb36dc606b8ee266d5c1f2e4a`;
  schema 78/save format 68 serialize the four window bytes in a 1,787-byte
  checkpoint, and the repinned replay remains cross-platform deterministic.

## Completed and live-qualified: UCF SDI, shield-SDI, and tumble boundaries

- [x] Reuse the damage-response, surface-response, schema-2 input-memory, and
  `native-csv-trace-v1` infrastructure. The extension adds three physical
  acquisition modes but no checkpoint protocol, public state, web probe,
  allocation, or direct production-state mutation.
- [x] Prove the exact strict inequalities consumed by UCF 0.84: raw main-stick
  delta 62 rejects and 63 accepts for both ordinary SDI and shield SDI; raw X
  delta 75 rejects and 76 accepts the DamageFall tumble wiggle. Every retained
  row includes processed input, explicit source raw input, ordinary/UCF ages,
  hitlag or tumble state, and the source action.
- [x] Pin independent hitlag captures at SHA-256
  `fb636ab13fd6ecdcb8f10f11af3640b8fbf6759a18f7e5ce01e9a7904f581b6e`
  and `612a9eb7e72adb7b4119246242ff511613be40eb4d12a83b112e8aa0a7a0b38b`,
  plus independent tumble captures at
  `3737ce0d2006f6d64d44e498499877844c3d0a938d0be42c53818f355b287bd0`
  and `473881a96ad47339e2e22f74eefdf1b63930ea2f98cf45d6f7901e312131d84f`.
  Hitlag permits only the explicitly checked one-Q16 displacement rounding;
  tumble source and production are byte-identical.
- [x] Keep the fast gate small. Final primary hitlag/tumble captures report
  7.046/4.459-second warm and 14.411/15.298-second cold totals. Focused stored
  CTests pass on MSVC and GCC, and the complete 35-domain / 218-case registry
  remains below its two-second budget on both platforms.

## Completed and live-qualified: UCF DBOOC and shield-input boundaries

- [x] Reuse the generic special-acquisition and native-CSV lanes. The capture
  route adds one reusable `platform_guard` acquisition and one composed
  input-plus-surface probe mode; it does not add a web probe, checkpoint
  protocol, runner, public state field, allocation, or action-specific table.
- [x] Prove seven strict source boundaries on pinned UCF 0.84: DBOOC adjusted
  radial over/under and X-age 1 control; shield-drop spotdodge suppression at
  raw main Y -63 versus -64; and extended pad-buffer delta 50 count 1-to-2
  versus strict delta 44 count 0. Every platform case independently proves
  Battlefield pass-through floor line 2 immediately before the edge.
- [x] Pin two independent 137-row captures at raw SHA-256
  `ebe6fe613e4e691adc8fec8f168ade7025ae1898544de1e084c2e59f42475874`
  and `493fe81f07dbcfeeb1674d959f3ed9db1416062e52450f33b379fe1928c58812`.
  Their canonical source projections and production are byte-exact at
  `73198f0ee5ab242d72598c4fa149d6f13e60112d69ddeb1d1f83e0218683c009`.
  The isolated stored gate takes 136.530-181.125 ms on Windows and 384.845 ms
  in WSL; the complete 33-domain / 212-case registry stays below 1.1 seconds.

## Completed and source-verified: animation-before-IASA callback ownership

- [x] Audit the common fighter process order and terminal animation callbacks.
  Melee runs animation before IASA; a terminal animation callback that calls
  `Fighter_ChangeMotionState` installs the successor state's IASA callback for
  that same update. The affected represented families are Squat/SquatRv,
  Landing/LandingAir/LandingFallSpecial, RunBrake/Turn/TurnRun/Dash, Appeal,
  GuardSetOff/GuardOff, EscapeF/B/N, ordinary ground attacks, and
  AttackAir-to-Fall. AttackLw3 correctly targets SquatWait rather than Wait.
- [x] Centralize the behavior in one allocation-free, stack-local callback
  owner projection. Existing input routers consume the projected action and
  clock, while one entry-effects block retires the source action's dash phase,
  attack collision epoch, smash-charge bookkeeping, rapid-jab tail, or
  GuardSetOff stun state. No canonical field, allocation, snapshot byte,
  replay field, or duplicated input router was added.
- [x] Cover the exact boundary update, not merely the following idle row:
  Squat-to-SquatWait can Dash, SquatRv/locomotion/landing/escape/Appeal targets
  can use Wait's direct EscapeN, TurnRun selects Run or Wait from the held
  target, GuardSetOff immediately runs Guard IASA, AttackLw3 targets SquatWait,
  a terminal jab targets Wait, and AttackAir immediately accepts Fall's
  neutral special. Collision/stun cleanup is asserted. Strict Windows MSVC
  and WSL GCC `m4_movement_test` both pass.
- [x] Causally repin the deterministic replay rather than accepting hash drift.
  Checkpoints through 62 remain identical to the pre-projection build. On
  zero-based input tick 62, P1 is at terminal Forward-Aerial Landing frame 18
  and the fixture's explicit ticks-35-through-80 segment holds full right.
  Melee installs Wait before IASA, so that input enters Walk immediately;
  the old sim emitted an artificial Idle row. Windows and WSL agree on the
  new 42,555-byte corpus/final/event hashes, and the event stream contracts
  from 84 to 82 by removing artificial action transitions.

## Completed and source-verified: direct EscapeN callback ownership

- [x] Audit the pinned common callback graph rather than treating every
  shield-startable grounded state as Wait. `ftCo_Wait_IASA` calls direct
  EscapeN before Guard; released grounded `ftCo_Damage_IASA` and imported
  attacks with the Wait callback policy delegate to that same table. Walk,
  Turn, Dash/Run, Landing, Squat, SquatWait, and SquatRv call Guard without a
  direct EscapeN callback. Falcon Appeal declares that callback but its two
  action scripts never enable `allow_interrupt`, so it has no live window.
- [x] Centralize the source-action capability in one allocation-free predicate
  keyed to the original action, before same-tick normalization. Wait, released
  grounded Damage, and eligible imported Wait-policy attacks may enter
  SpotDodge immediately; other grounded actions enter the existing GuardOn
  path and can reach EscapeN from Guard on the next update.
- [x] Cover the observable split directly: Wait plus fresh shield/down enters
  SpotDodge, while identical input from CrouchStart and Walk enters Shield.
  Strict Windows MSVC and WSL GCC `sim.m4_movement` both pass.

## Completed and source-verified: DamageFall attack callback envelope

- [x] Audit pinned `ftCo_DamageFall.c` at normalized SHA-256
  `62d858d009bd79855a1c6d7ce8f371e78522af6ea7c9641d7c8b7af410dde0d0`.
  Its released DamageFall IASA checks aerial special/item routes and usable
  aerial jump before the wiggle callback; it does not dispatch ordinary
  aerial attack or EscapeAir.
- [x] Remove ordinary A/C-stick from the wiggle-preemption mask and gate the
  shared aerial-attack entry on non-tumble state. This is a pair of local
  callback predicates with no new state, allocation, content field, ABI,
  snapshot byte, or duplicated transition.
- [x] Cover A-alone and C-stick-alone rejection plus a strict UCF raw-delta
  wiggle combined with A. The first two remain DamageFall/tumbling; the last
  enters ordinary Fall and clears tumble without entering an aerial attack.
  Strict Windows MSVC and WSL GCC `sim.m4_movement` both pass.

## Completed and live-qualified: early basic-Turn interrupt facing

- [x] Follow pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` and repository-current revision
  `d882af94175e3c880ad51039e2979aa9a50aea09` through `ftCo_Turn_IASA`.
  Both revisions have normalized `ftCo_Turn.c` SHA-256
  `3ad604c90ae3f67dd508cced55ab00ca6e7152a4a15693c5c78d4959434cbcfa`.
  Before Turn's physical facing flip, its special/grab/attack callback prefix
  temporarily sees the pending facing; guard/taunt/jump run only after the old
  facing has been restored when that prefix rejects.
- [x] Reuse one stack-local callback-facing view across the existing special,
  grab, and attack routers, including main-stick and C-stick smash selection.
  The correction adds no persistent state, content field, parser branch,
  allocation, snapshot byte, or duplicated action transition.
- [x] Qualify 12 checkpoint-isolated cases / 72 live rows: jab, grab, all three
  main-stick smashes, all three tilts, all three C-stick smashes, and jump as
  the post-restoration negative control. Two captures have identical rows and
  raw SHA-256
  `294e9eae84ae6bc92f5932e20d666479b4455e49bb9666a54052003ec94b2c59`;
  source and Windows/WSL production match exactly on action, action tick,
  facing, and grounded at SHA-256
  `0e9858e16140d8a55727255b009aec89e2176286e008b2cf23867bab38c2ac44`.
- [x] Register the same 12 cases / 72 samples through the reusable native-CSV
  stored lane. The generated artifact SHA-256 is
  `a0f269dd6683e372629190bfa011f35f79be226fc6a2a150c49eebc55961b8a0`;
  focused passes take 304.855/307.646/347.483 ms on Windows and
  376.784/415.305/458.985 ms in WSL. The 31-domain / 186-case registry passes
  three isolated Windows runs in 1485.200/1706.950/1922.805 ms and three warm
  WSL runs in 1096.947/1127.462/1220.305 ms under manifest SHA-256
  `ffb5f801f55e24ce9c7a94fcbc627e46b1bcf0a42ebb79db37d746a1c9938664`.
- [x] Requalify deterministic replay at the causal boundary rather than merely
  updating hashes. Checkpoints through completed tick 217 are identical;
  input tick 217 is early basic Turn plus A and forward stick. The old path
  incorrectly entered jab facing left, while the corrected source path enters
  forward tilt facing right. Windows and WSL now agree on corpus/final/event
  SHA-256 values `6727023fb07bcb7a4fcbaf9c0beac0f8220c1c1802b19da891ae2ae2be252240`,
  `de96572115c1e4850d79353839576efc4b780ccbd75e8e70a2f23bee419c14af`, and
  `124a94734029321020513ec749b2f4d26cd60b4ed2129e25ce104692739fa9af`.

## Completed and live-qualified: Run-to-RunBrake acquisition

- [x] Follow pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` and current upstream
  `d882af94175e3c880ad51039e2979aa9a50aea09` through ordinary Run, RunBrake,
  and the locked Run phase after TurnRun. The captured split is explicit:
  straight full down from Run enters RunBrake, radial-gate diagonal down stays
  in Run for the edge row, down from RunBrake enters CrouchStart, and locked
  post-TurnRun Run rejects direct down. Normalized source SHA-256 values are
  `72a9ce8c19948d468f6aea484b72db3b1f0c280846adc4d5677e4c6a20b810fe`
  for `ftCo_Run.c`,
  `0c75e6a95319f2be3a42dcade65b07671d47d7a31e7191e04cb617fce13866bb`
  for `ftCo_RunBrake.c`, and
  `80c2e71e50622e942754bfcdd3bd89f3762fe4df2400d8055f059ab6cc4b8082`
  for `ftCo_Squat.c`.
- [x] Remove Run from the generic direct-crouch predicate so it reaches the
  existing shared RunBrake transition; retain the existing RunBrake-to-crouch
  callback. This is an allocation-free predicate correction with no new state,
  table, parser field, snapshot byte, or duplicated movement path.
- [x] Qualify four immutable checkpoint cases / 127 live rows. The two captures
  have identical ordered rows and raw artifact SHA-256 values
  `1d3c568f38f6dcd359e77c3b1616a6e7d81480dff4e8b3aa5262e528533fd8b9`
  and
  `e74a8c0ecc7628ba2886e7ad10b4633d2e1ad0eac5ecf6c5ec86f057a9d1ab16`.
  Warm work takes 0.541609/0.311504 seconds; complete lifecycles take
  6.177062/3.647950 seconds. Source and production canonical payloads are
  exactly equal at SHA-256
  `dfa7be0339110c98c9107a069ef7e9751b14f2c174bd04a7e977c90ae745f6ad`.
- [x] Register the same four cases / 127 samples through the reusable native-CSV
  stored lane. The focused domain passes in 263.089 ms on Windows and 403.007
  ms in WSL.
- [x] Qualify the complete 30-domain / 174-case registry in isolated runs. Three
  Windows passes take 1178.830/1319.197/1471.076 ms; three WSL passes take
  919.397/986.464/1270.306 ms under manifest SHA-256
  `99b5f633b2f4f6c33173ca285af0634e0ac51d1acc6df8b2a5b3c57f22cb261d`.

## Completed and verified: released Damage versus DamageFall air dodge

- [x] Audit pinned `ftCo_Damage.c`, `ftCo_DamageFall.c`, and
  `ftCo_EscapeAir.c`. Released ordinary airborne Damage delegates to the
  ordinary Fall IASA table, which includes EscapeAir; DamageFly/DamageFall's
  dedicated callback table omits EscapeAir while retaining its other sourced
  branches.
- [x] Remove the broad damage-action exception from the existing digital-
  trigger predicate. The shared allocation-free transition now keys on the
  already-canonical tumble bit: non-tumble Damage remains eligible and
  DamageFall remains ineligible, with no new state, content field, or duplicate
  air-dodge path.
- [x] Qualify two physical cases / 68 live rows. Fresh L enters AirDodge from
  released non-tumble Damage; the same edge leaves DamageFly/DamageFall in
  TUMBLING for two airborne observations. Source semantic SHA-256 is
  `7ce52b784989e56f7539b79dd779eed94ab41e4bcd624b980c263af0b916084b`;
  production canonical SHA-256 is
  `cec3d2b1d9b67ad906bf68b074c8975f6e53bf48cc714650c6498bef7aeba93e`.
  Warm captures take 0.587707 and 0.897459 seconds; the second complete
  lifecycle takes 4.981875 seconds.
- [x] Register the two-case/four-sample numeric trace. The complete 29-domain /
  170-case registry plus replay passes in 1.286 seconds on Windows and 1.188
  seconds in WSL. Windows serial CTest passes 40/40 in 8.42 seconds and WSL
  passes 42/42 in 9.82 seconds.

## Completed and verified: callback priority and direct source equality

- [x] Correct SquatWait/SquatRv's special mask to down+up after following the
  body of `ftCo_Attack100_CheckInput`. Four axial/diagonal cases cover both
  crouch phases; two byte-identical 106-row captures have raw SHA-256
  `527980419abfc7afdf7b698e65be21b0ed31e70a94a3443abd0a425da9ab29f4`
  and exact source/production semantic SHA-256
  `3117c2767a723556602b43caf5b34cd9a0376f854adcd3f0f4f49d7c1c11bba6`.
- [x] The resulting 28-domain / 168-case registry plus replay passes in
  1.293 seconds on Windows and 0.979 seconds in WSL.
- [x] Port KneeBend's misleadingly named `ftCo_Attack100_CheckInput`
  callback as the up-special-only dispatcher that runs before Catch and
  UpSmash. Three natural input cases prove grounded Falcon Dive acquisition,
  up-B priority over simultaneous Z, and side-B rejection without direct
  action writes. Two byte-identical 18-row captures have raw SHA-256
  `7523884c819b8ad371139b020cff562a0a0d4786cef1fde4a4dff2d499d42d51`;
  source and native production share canonical SHA-256
  `0695488cb8bff660bfabe69298f366ed7bbbfed4348330636b04f87bff43aa17`.
  The generic acquisition verifier now supports exact heterogeneous edge-row
  sequences, so rejected callbacks can remain in their ordinary source action
  without a character-specific verifier.
- [x] The complete 27-domain / 164-case plus replay registry passes in
  1.614 seconds on Windows and 1.385 seconds in WSL. Both full Release suites
  pass 41/41; focused WSL ASan/UBSan movement passes.
- [x] Qualify `ftCo_SpecialAir_CheckInput`'s neutral-special turnaround from
  the imported `x224 == 20` strict input-age window and remembered horizontal
  direction. Three natural jump/flick/neutral/B cases prove reversal at age
  19, rejection at age 20, and the same-direction negative control. Two
  byte-identical 91-row captures have raw SHA-256
  `3d4bb6c4a7cde8d2879e846eecf7e2fc3ca0d5151eb466fc7760678c83f58ad9`;
  source and native production share canonical SHA-256
  `027fad335436a97393260b553019fe6247661b3ae1c03d981b4b1db4cc4d5fcb`.
  The capture schema now accepts reusable recorded pre-edge phases, so causal
  input history remains inside the identical-input theorem instead of a
  character-specific pre-roll.
- [x] The complete 26-domain / 161-case plus replay registry passes in
  1.191 seconds on Windows and 1.065 seconds in WSL.

- [x] Audit pinned/current `ftCo_Wait_IASA`, `ftCo_Walk_IASA`,
  `ftCo_Ottotto_IASA`, `ftCo_GuardOff_IASA`, and Falcon's special collision
  callbacks. Walk checks Catch before Special, while Wait checks Special before
  Catch and attacks. Generic charge, reflector, and projectile frontends no
  longer consume reference-Falcon B inputs before that state-specific table.
- [x] Add physical simultaneous-input and Dash-direction discriminators. Walk
  Z+B enters Grab; low non-neutral Walk B enters Falcon Punch; Wait Z+B enters Falcon
  Punch; Wait up+A+B enters Falcon Dive; and Dash exposes only side-B while
  up+B falls through to running tap jump. Sixteen isolated cases / 188 rows
  repeat byte-identically at raw SHA-256
  `f92c89a2108d880746bf66d286d42dfcfcb5ad87eee425dec22cdf933115e4cc`
  and canonical SHA-256
  `8fbfbcb12c5cdb483891315a4dc4c57a642c28ae2eb8ad886b31fecf9d3cd03d`.
- [x] Add the reusable live/native canonical comparator and exact-source gate.
  It exposed and closed Turn-special facing, Teeter Falcon Dive acquisition,
  Falcon Punch's decomp mode-2 ledge clamp, and GuardOff EscapeN/attack entry
  clocks. Common acquisition, Teeter, and GuardOff now have identical source
  and production semantic digests on Windows and WSL; the stored verifier
  rejects any attempt to pin a divergent production digest as equivalence.
- [x] The complete 25-domain / 158-case plus replay registry passes in
  1.214 seconds on Windows and 1.771 seconds in WSL. Windows and WSL Release
  pass 41/41; focused WSL ASan/UBSan passes movement, combat, projectile,
  reflector, and charge.

## Completed and verified: GuardOff callback acquisition

- [x] Sweep pinned decomp `9509dc0` and current upstream `d882af9`.
  `ftCo_GuardOff_IASA` exposes the full special/attack/grab dispatcher only
  while GuardOff's powershield work flag is set, then checks spot dodge and
  button/tap/C-stick jump for both ordinary and powershield release.
- [x] Extend the existing allocation-free per-action capability abstraction.
  `PF_M4_ACTION_SHIELD_RELEASE` receives all four special callbacks only from
  the already-derived powershield release-cancel condition. No new rollback
  field, table, allocation, or duplicate special router was added.
- [x] Qualify the complete powershield-only dispatcher—four special
  directions, jab, all three tilts, all three smashes, and grab—plus one
  ordinary-shield special negative and ordinary/powershield jump plus spot
  dodge through physical Falcon Jabs.
  The fixture resets both fighters'
  complete current/previous collision-position histories and applies the
  defender's shield edge begins with the causal attack and is reaffirmed at
  the opponent's observed Jab frame with unbatched EXI input. Six concurrent
  workers retain at most three divergent physical cases each. Two 119-row
  captures are byte-identical at raw SHA-256
  `20e3d7a2e5e5cba93df059069b72cf560a0c4641258582997fd6aebc6bdc8649`;
  source and production share semantic SHA-256
  `851a0c05e393bd644344bf8a49d70fceea179727903ff68feacebb1c12a27c0d`.
  Warm captures take 7.867 and 7.607 seconds; complete cold runs take 9.103
  and 7.937 seconds.
- [x] Register the generic native-CSV domain. The complete 25-domain /
  158-case plus replay gate passes below two seconds on Windows and WSL.
  The focused 17-case lane takes 0.880/0.417 seconds.
  Both post-rename Release suites pass 41/41; focused WSL ASan/UBSan
  movement/combat passed for the production implementation.

## Completed and verified: teeter special acquisition

- [x] Sweep pinned decomp `9509dc0` and current upstream `d882af9`.
  `ftCo_Ottotto_IASA` calls the full common special table, and
  `ftCo_OttottoWait_IASA` delegates directly to it. Production's single
  `PF_M4_ACTION_TEETER` representation therefore covers both source phases
  with the full four-bit special capability mask.
- [x] Build a natural endpoint fixture on both sides of the equivalence
  boundary. Dolphin uses an unrecorded low-stick walk followed by neutral to
  enter actual `EDGE_TEETERING_START`. The allocation-free native pre-roll
  selects a sub-dash walk input from endpoint distance, current velocity,
  traction, and acceleration; neither runner writes an authored teeter state.
- [x] Qualify neutral/side/up/down B as four isolated actual-input cases / 28
  rows. Each source route records teeter frame 1 followed by the acquired
  grounded special on frames 1-6. Two captures are byte-identical at raw
  SHA-256 `a21b615da3f45642278ce4a1b2f6ba8335588e2568e423b2467fd1d55119bcca`;
  source and production share semantic SHA-256
  `2065e789ba0285f8b3d878bdc2615bf0a7e983ee02da356f6f46d0b924a6908e`.
  Warm capture times are 0.268 and 0.245 seconds.
- [x] Register the generic native-CSV domain. The complete 24-domain /
  133-case plus replay gate passes in 1.124 seconds on Windows and 1.271
  seconds in WSL. Windows Release passes 41/41, the WSL Release configuration
  passes all 35 enabled targets, and focused WSL ASan/UBSan movement/combat
  pass.

## Completed and verified: common special acquisition masks

- [x] Sweep pinned decomp `9509dc0` and current upstream `d882af9`. Both show
  that `SquatWait` and `SquatRv` call only `SpecialLw`, while `Turn` calls
  `SpecialS`, `SpecialLw`, and `SpecialHi` but deliberately omits `SpecialN`.
- [x] Replace the coarse per-action special boolean with an allocation-free
  four-bit callback capability mask. Input priority masks unavailable
  callbacks before resolving diagonals, so a side component cannot suppress
  SquatWait/SquatRv's allowed down special. Authored non-reference routes keep
  their prior full dispatcher.
- [x] Repair the checkpoint boundary exposed by the new divergent branches.
  Protocol v2 owns sixteen fixed one-shot slots, saves eight neutral Wait
  snapshots, consumes each load on an unrecorded neutral EXI frame, and only
  then submits the recorded case input. This removes pre-restore input and
  state contamination without reconnecting Slippi or relaunching Dolphin.
- [x] Qualify sixteen actual-input cases / 188 rows: Turn rejects neutral B and
  accepts side/up/down B; SquatWait and SquatRv accept straight and radial-gate
  diagonal down B; Walk and Wait distinguish Catch/Special priority; and an
  A+B up-special chord cannot leak into authored charge. Dash accepts only
  side-B; neutral/down-B retain Dash, and up-B falls through to the running
  tap-jump callback. Two fresh captures
  are byte-identical at raw SHA-256
  `f92c89a2108d880746bf66d286d42dfcfcb5ad87eee425dec22cdf933115e4cc`;
  source and production share semantic SHA-256
  `8fbfbcb12c5cdb483891315a4dc4c57a642c28ae2eb8ad886b31fecf9d3cd03d`.
  Warm capture times are 2.529 and 3.092 seconds.
- [x] Register the reusable native-CSV domain. Focused Windows/WSL stored
  gates pass in 0.360/0.463 seconds including replay; both native movement
  suites pass their 20,000-tick deterministic route. The complete 23-domain /
  129-case plus replay gate passes in 1.190 seconds on Windows and 0.988
  seconds in WSL; both 41/41 Release matrices and focused WSL ASan/UBSan
  movement/combat pass.

## Completed and verified: grounded slope damage launch and recontact

- [x] Sweep pinned decomp revision `9509dc0`, current upstream `d882af9`, the
  extracted `PlCo.dat`, the existing Hyrule line catalog, and registered
  collision oracles before implementation. The governing `ftCo_Damage.c`
  route is unchanged upstream and no parallel prior-art runtime supersedes the
  existing fixed-point stage adapter.
- [x] Import common-data `x1E8` (10 degrees) and `x1EC` (0.8). The shared
  fixed-point resolver converts the anisotropic simulation vector back to
  Melee coordinates, performs the floor-normal angle test, preserves the raw
  `xF0_ground_kb_vel` scalar, projects grounded `x8c_kb_vel`, and applies the
  strict level-three steep-floor vertical reflection without runtime float.
- [x] Preserve source callback order and vector ownership. Damage motion
  selection consumes the original pre-projection launch vector; a
  ground-origin airborne launch installs `ftCommon_8007D5D4`'s ten-update ECB
  bottom lock before hitlag; `Landing` keeps the incoming air `x8c` vector on
  its entry row and begins floor projection on the following grounded physics
  update. The common air-entry adapter now correctly converts collision-sweep
  extent back to root-space bottom before storing a lock.
- [x] Add an at-will two-case/60-row identical-input Hyrule line-36 theorem.
  Opposite Forward Tilts against a crouch-cancelled Falcon qualify grounded
  projection and airborne departure plus same-update downhill recontact.
  Independent captures have identical observation rows and semantic source
  SHA-256 `657b816faa98658d10be6783b912a380cf88c24ccc1120d0a5836f61e6aa6ac9`;
  production SHA-256 is
  `15b3705d0c7a6e9c83d3a540c6b90da4af835676011a2726fdb360a3e8fdf05e`.
  Warm capture times are 0.806 and 0.738 seconds.
- [x] Register the domain in the fast gate. Twenty-two domains / 119 cases
  plus replay pass in 1,707.169 ms on native Windows and 1,035.323 ms in WSL,
  below the two-second budget. Rebuilt Release suites pass 35/35 on both
  platforms. The intentional ECB-lock serialization changes only the replay
  corpus SHA-256 to
  `7f210b0b70d2a506f60da411d4212885a5714ddc816c6fb076ad6273939a5ef0`;
  final-state and event digests remain unchanged.

## Completed and verified: floor-response knockback channel ownership

- [x] Sweep pinned decomp `9509dc0` and current upstream `acfb24e`; the
  governing Damage, Passive, PassiveStand, DownBound, fighter, and common
  ground-transition routes are unchanged.
- [x] Preserve the source's four distinct landing policies. Basic Landing and
  directional tech retain the complete incoming air `x8c` vector with `xF0`
  zero on entry. Neutral tech and missed tech/DownBound initialize and clamp
  `xF0`, then project `x8c` immediately. Ground decay begins on the following
  update in every branch.
- [x] Extend the existing actual-input Final Destination oracle rather than
  adding a duplicate probe. Its 804 live rows prove that forward/backward tech
  retain vertical knockback for exactly frame one, while neutral and missed
  tech are already projected; all four are projected on frame two. The
  production trace now includes and checks `xF0` ownership.
- [x] Requalify affected stored domains. Floor response, grounded-slope
  damage, and Hyrule slope/ledge production digests are respectively
  `47ebff88692b3344c5e2cf24e790763c572d78e687be17e3d78a09e8e875f04a`,
  `15b3705d0c7a6e9c83d3a540c6b90da4af835676011a2726fdb360a3e8fdf05e`,
  and `bf8b2f390b2246835678a49ce191120ac4b8f39a4fb82130e9df5675354ac8a4`.
  The full 22-domain / 119-case plus replay gate passes in 1,485.949 ms on
  Windows and 1,936.815 ms in WSL; replay corpus/final/event digests are
  unchanged. Rebuilt Release suites pass 41/41 on both platforms, focused WSL
  ASan/UBSan combat passes, and the cross-platform verifier soak golden is
  `f965394d7f9f082a` with all counters unchanged.

## Completed and verified: non-tumble damage floor selector and lifecycle

- [x] Sweep pinned decomp `9509dc0`, current upstream `d882af9`, extracted
  `PlCo.dat`, and the existing Hyrule physical-damage pack. The relevant
  `ftCo_Damage_Coll` route is unchanged upstream; the existing pack already
  exposes every required source field, so no web probe or new protocol was
  added.
- [x] Import exact common-data `x1E0=5.0` and `x1E4=0.5`. One allocation-free
  fixed-point selector undoes world anisotropy and compares squared isotropic
  magnitudes: forced or at least 5.0 enters DownBound, at least 0.5 enters
  basic Landing, and below 0.5 retains the current Damage action.
- [x] Preserve `Damage_Anim` ownership after hitstun release and floor contact.
  The retained route keeps its selected source submotion and clock until the
  imported terminal frame while ground physics independently clears/projects
  its velocity channels. Authored non-reference behavior remains unchanged.
- [x] Extend the existing checkpoint pack with actual Falcon Jabs rather
  than synthetic velocities. The terminal route lands on sample 16 below
  0.5, stays in DamageN2 through sample 24, and enters Wait on sample 25. A
  second route presses B on released grounded Damage frame 15 and enters
  Falcon Punch on sample 17 through `ftCo_Wait_IASA`. Two fresh captures pass
  all four cases / 120 rows with semantic source SHA-256
  `2ad67d79ef1fa278e5ea55096b663b0e59793167161eedc870e4c7663fe7a6a5`;
  production SHA-256 is
  `cb0b203a0a211baa55b800cd9e0cf0eb8e4595eaa069c8e865369cad8c94de61`.
  Warm capture times are 2.295 and 2.171 seconds.
- [x] Pass the complete 22-domain / 121-case stored-plus-replay gate in
  1,142.906 ms on native Windows and 894.651 ms in WSL, below the existing
  two-second budget. Full Windows and WSL combat tests pass; sanitizer and
  complete CTest evidence are recorded in `docs/milestones/M4_progress.md`.

## Completed and verified: mixed hit-entry ECB ownership

- [x] Preserve the production pipeline's existing source order: movement and
  map collision complete before attack collision resolves a hit. No duplicate
  rollback field or sampled ECB table is needed for the observed entry row.
- [x] Extend the generic HSD action verifier with declarative
  `previous-row-post-animation` ECB ownership. Hurt still evaluates from the
  row's current DamageN2 submotion/frame; ECB evaluates the preceding Wait
  variant at `previous_frame + previous_rate` with its callback-facing owner.
- [x] Qualify six mixed rows in each of two independent physical captures.
  The complete damage-pose theorem now covers 288 observations / 3,168 hurt
  capsules, including 12 mixed entry ECBs, at maximum one-Q16 error.
- [x] Record the callback-order rule in the reusable character-importer skill
  so later ports do not discard or misattribute split-ownership rows.

## Completed and verified: DamageFlyTop / DamageFlyRoll

- [x] Sweep pinned decomp revision `9509dc0`, current upstream `d882af9`, and
  the existing DAT/HSD importer before implementation. The relevant current
  `ftCo_Damage.c` and motion-table source is unchanged; no maintained prior-art
  implementation supersedes the existing allocation-free evaluator.
- [x] Import common-data `x234/x238/x23C/x240` as the strict 70/110-degree
  top cone, 100% Roll threshold, and exclusive u16 RNG boundary. Combat now
  uses the transaction-local shared HSD RNG, so rollback commits one canonical
  stream and ineligible hits consume no draw.
- [x] Import Falcon raw submotions 180/181 and extend the immutable parent-closed
  HSD profile to 68 motions, 4,511 tracks, and 44,881 keys under decoded-data
  SHA-256 `4994dfb44a97051627fb557c8f371f047d2e28cd5672c9b4c4a2aa143aa82ad3`.
  `DamageFlyRoll` overwrites retained joint `XRotN` with
  `facing * atan2(total_x,total_y)` in Melee source coordinates before the
  same local pose feeds hurt capsules and ECB collision.
- [x] Reuse the existing accelerated surface/prone checkpoint evidence instead
  of adding a web probe. A fresh response-only run contains `DamageFlyRoll`
  and passes all 145 response rows across five cases at
  observation SHA-256
  `5339134dd04cff9612e8c8a3e1d460f85018ae4c081ac7426fbad3cee3b785f5`.
  Derived world-space stage fields were removed from oracle hashing because
  they duplicate the pinned raw line table and caused schema-only churn.
  The single-process wrapper remains above its strict three-second warm budget
  (4.234 seconds); the generic forked runner regresses end-to-end startup by
  booting multiple Dolphins, so persistent-process reuse remains open.
- [x] Pass Windows Release 41/41 in 6.22 seconds and WSL Release 41/41 in
  4.94 seconds. The 21-domain / 117-case stored-plus-replay gate passes in
  1.081 seconds on Windows and 1.011 seconds in WSL. Both generated-data
  `--check` gates pass.

## Completed and verified: complete base/secondary Wait lifecycle

- [x] Sweep current and pinned `doldecomp/melee` plus current HSDLib before
  implementation. `ftCo_Wait_Anim` ends base Wait through
  `ftCo_8008A7A8`; `ftwaitanim.c` selects secondary waits through the global
  `HSD_Randi` stream. No maintained prior-art project owns that complete
  runtime selection lifecycle.
- [x] Add one reusable focused capture boundary that enters Wait naturally via
  `SquatRv`, then records exactly source frames 0-59 before variant selection.
  Control/repeat raw SHA-256 values are
  `2a7a0d34e06655d9528bf64180fd053c154855f59184f6a88d81ed991e18ac3d`
  and `e5584ca5e52dd0b7c3d54d9f16e45fe08ad74c0b00268b8f1fc0269bfabe7ca8`;
  their address-free semantic projections are identical at
  `f2a336241e781472352210197d444b1dbe94b2d4b9ccf6f008f5d8efec079489`.
- [x] Qualify the source motions and transition independently. Across all ten
  SquatRv observations and direct Wait frames 6-59, the generic verifier checks
  128 observations / 1,408 hurt capsules plus ECB and reports maximum one-Q16
  coordinate error. Two surface-memory captures then prove Wait frames 0-5 as
  the six-update moving-target recurrence, including SquatRv source frame 10.
- [x] Reuse the canonical source-submotion/frame/rate fields and shared compact
  HSD evaluator. Ground idle advances Wait at rate 1. A 20-byte replay
  descriptor reconstructs wide-SRT source poses on demand instead of growing
  canonical rollback state by a full translation pose; six generated transition
  observations protect the natural production route's hurt capsules and ECB.
- [x] Pass Windows Release 40/40, WSL Release 40/40, and WSL ASan/UBSan 26/26.
  The 21-domain / 117-case stored-plus-replay gate passes in 0.970 seconds on
  Windows and 0.925 seconds in WSL. State schema 75 keeps the 1,747-byte save
  format and 42,519-byte replay size unchanged.
- [x] Import Falcon's DAT-owned 70/20/10 Wait/Wait2/Wait3 table and 6/0/0
  blend bytes, plus exact process-global HSD LCG state and retry ordering.
  RNG draws are transaction-local until a tick commits; RNG version 2 makes
  the behavior contract explicit without adding per-player state.
- [x] Capture two 440-row checkpoint-isolated lifecycle runs. Both raw files
  and their address-free semantic streams are identical at SHA-256
  `d97474f2a15912b1c98fba9b7444883c1db4798290702c311b13bcffb4cc7f7b`
  and `afefafe17e8769bc39391d0605d7c392f25ef4d146cf0d868eb895eeee84b570`.
  The verifier checks 880 live rows and a separate uninterrupted source theorem
  of 14 RNG draws and two same-secondary rejections, plus all three weights and
  exact 60/75/70 frame lengths.
- [x] Import every Wait2/Wait3 HSD track into the shared allocation-free
  evaluator. All 145 direct variant poses / 1,595 capsules plus ECB agree with
  Dolphin within one Q16 unit. Thirty-two stored lifecycle poses cover both
  entries, middle/terminal frames, and every return/restart blend update.
- [x] Add `tools/verify_ssbm_falcon_wait_lifecycle.sh` for fresh capture,
  source/DAT/RNG verification, dynamic hurt/ECB qualification, generated-data
  checks, and a focused `sim.m4_ssbm_falcon_wait_idle_lifecycle` native gate.

## Completed and verified: 20 ordinary-action collision ECB routes

- [x] Reuse the existing paired 2,974-row live hit/hurt capture instead of
  adding another web probe or Dolphin route. Its independent raw captures are
  pinned at SHA-256
  `aeff75c16b2041fbecc6b8ec2322a614e0695f0d3d9088eb44d60aedbdeb7ca0`
  and
  `5a797d05fe1dfd30ee1a82b7ede3cac3c003a668d20dcd1d53b824450e19bd55`.
- [x] Extend the generic DAT/HSD source qualifier to manifest-selected case
  groups and generated per-motion displayed-frame offsets. All 20 qualified
  routes pass across both captures: 1,676 observations / 1,450 unique frames,
  with maximum one-Q16 coordinate error.
- [x] Route Jab 1/2/3, Rapid Jab Loop/End, Dash Attack, five forward tilts,
  Up/Down Tilt, the three real forward-smash angles, Up/Down Smash, and both
  grabs through one action-to-imported-move adapter and the shared
  allocation-free HSD evaluator. The adapter adds no runtime table, parser,
  allocation, floating point, or rollback state.
- [x] Generate the action-frame offset switch from the import manifest as its
  single source owner. The shared parent-closed profile now contains 45
  motions, 2,976 FObj tracks, and 32,285 keys under decoded-data SHA-256
  `17da37dd9cdb080559407a7b8268bc52a590063bf9c84ef9b34e2de324e78dee`.
- [x] Exercise all 725 then-qualified production action frames in the focused
  native HSD test. This intermediate slice rejected Rapid Jab Start plus all
  five aerials; the later common-air-lock slice below closes the five aerials
  without weakening that rejection boundary.
- [x] Pass Windows MinGW Release 39/39 in 4.94 seconds and WSL Release 41/41
  in 3.22 seconds. The complete 21-domain / 117-case stored-plus-replay gate
  remains green in 999.624 ms on Windows and 855.897 ms in WSL.

## Completed and verified: ordinary guard hurt/ECB geometry

- [x] Trace `ftCo_Guard.c` instead of guessing from the action names.
  `GuardOn` and `Guard` enter with `Ft_MF_SkipAnim`; `ftCo_80091E78`
  manually blends Wait toward the shield skeleton for eight updates and then
  freezes the terminal pose. `GuardOff` is the ordinary 16-frame HSD motion.
- [x] Add reusable sequential-frame and fixed-frame selectors to both bounded
  hurt and ECB extractors. Raw GuardOn HSD samples were rejected because they
  differed from live by 9,594-31,608 Q16; GuardOff matches within one Q16.
- [x] Capture two fresh 40-row guard routes. Their raw SHA-256 values are
  `e9141d1ce253bee82233d9545cf20145d594d60510cee5ea77b19ca5e12390b9`
  and `88943aab9a5d70c79570ab108f9a9183fd69d3a0bde8c9d2ee38a641a089b1ef`;
  both regenerate hurt semantic SHA
  `4db8c524835e969b5b34fda81e53b59d6af99aa68d13e7203086c6441a41abde`
  and ECB semantic SHA
  `a1bd5b9937cb342a053415ecc674b36dc5a01fb575ed688b32f8e097e1b209c1`.
- [x] Route production hurt inspection and collision through the same retained
  GuardOn/Guard/GuardOff source-submotion identity. The 25-pose profile adds
  275 capsules and 25 four-point ECBs without runtime parsing, allocation, or
  new rollback state. Snapshot validation preserves a receiving guard pose
  through hitlag and validates GuardSetOff only after shield-stun resumes.
- [x] Extend `falcon-common-hurt` to 714 poses. The fresh 16-case / 323-row
  checkpoint pack and exact dash hit/miss discriminator pass under source SHA
  `9688be9b0ca0d0eacac5ba26714968acdcc3b19aaac9449778c108275b1c940b`;
  the production accessor passes SHA
  `0641ed13ea1d179e214f5629b4f8d7b93e226091b9925d6e31dfe53645c74c36`.
  GuardSetOff's dynamic shield-stun pose is closed in the next section.

## Completed and verified: GuardSetOff dynamic hurt/ECB geometry

- [x] Trace `ftCo_80092F2C` at pinned and current decomp revisions. The
  callback starts ordinary GuardSetOff frame zero at
  `(0.1 + animation_endpoint) / unrounded_shield_stun_duration`; the imported
  Falcon endpoint is 20 frames. Integer shield-stun ticks remain a separate
  countdown.
- [x] Preserve source callback order without new canonical state. Hitlag owns
  the receiving GuardOn/Guard collision pose while retaining the new rate;
  the first resumed update switches to submotion 40 and advances by that
  fractional rate. Generic fighters without reference frame data keep the
  canonical zero clock and Wait identity.
- [x] Capture light, midpoint, and dense physical shield hits twice with
  headless/null/no-fast-forward ExiAI. All six 159-row captures produce the
  same semantic streams per pressure: 18 qualified updates / 198 capsules,
  exact hurt geometry, and maximum one-Q16 ECB difference. Control/repeat
  hashes are pinned in the import manifest.
- [x] Replay all six captures through the production movement/combat runner.
  The identical-input comparator now anchors at the explicit 22-Melee-unit
  placement theorem and checks the retained animation frame/rate through
  hitlag and recovery. All six 99-frame native comparisons pass, covering
  36 GuardSetOff clock rows. The runner uses the imported Jab 1 timing and
  source spheres; its obsolete authored two-active-frame override is removed.
- [x] Extend the shared HSD profile to 22 motions, 1,574 tracks, and 12,784
  keys under data SHA-256
  `386f7caf986b582363efc79aaf2efda04a93b812f9f3565ef62c6690eefe6e1b`.
  Nine generated stored observations cover all captured post-hitlag phases;
  production-path tests independently cover rate derivation, hitlag freezing,
  and the first resumed pose across all three pressures.
- [x] Windows Release and WSL Release pass 40/40; WSL ASan/UBSan passes 26/26.
  The 21-domain / 117-case stored-plus-replay gate passes in 0.887 seconds on
  Windows and 0.823 seconds in WSL.

## Completed and verified: shield-break animated hurt/ECB geometry

- [x] Sweep the pinned/current `doldecomp/melee`, HSDLib, meleeDat2Json, and
  meleeFrameDataExtractor revisions before extending the existing generic HSD
  importer. No maintained prior-art tool provides Melee's complete runtime
  fighter-to-simulator geometry path; the current decomp remains the behavioral
  authority and the owner-extracted DATs remain the animation authority.
- [x] Capture two independent 500-row headless/null/unlimited shield-depletion
  routes with the memory pose probe. Across both captures, all 42
  ShieldBreakFly, 26 ShieldBreakDownD, and 30 ShieldBreakStandD source frames
  are complete: 196 observations / 2,156 capsules agree with the independent
  HSD evaluator within one Q16 unit. Raw SHA-256 values are
  `ae02dca6e63eda47e780ee96cae26c4c8a565f4e2d534979791f827d737f5645`
  and
  `ddbc82fcd401fdbea41a202964f3dff678eff32a23199567607e26cb9ba5f40b`.
- [x] Expand the single parent-closed HSD profile to 21 motions, 1,514 tracks,
  and 12,609 keys under data SHA-256
  `a5edfc9fabbd3ed9c351fbe68b3a91c16e4954243ca14e3d7273baadc44fc2b8`.
  Production hurt inspection and collision consumers share the same
  allocation-free source evaluator; the old ground-loop-specific API name is
  removed.
- [x] Qualify hurt and ECB independently. The raw HSD matrices are exact for
  the hurt capsules but differ from live collision ECBs on this action because
  Melee applies runtime animation/collision callbacks and entry ECB locking.
  The importer therefore retains one compact 198-frame live ECB profile rather
  than pretending the raw selector joints are authoritative. Profile / semantic
  SHA-256 values are
  `2b4354f075594264ddb1686c9123c78459658a8dec145d78445c1b115585bc7c` /
  `11b28d22f68f7bb87c99dbc5f949f5456d1a69ab7bfa78360927ecb334064eeb`.
- [x] Correct the shared ECB clock path so retained source motions derive their
  frame from action ticks and the generic HSD resolver receives the actual
  grounded state. Three fresh identical-input shield-break comparisons pass
  all 500 frames under the registered strict-field and Q16 envelopes.
- [x] Extend the same split qualification to ShieldBreakStun/Furafura. Its
  mash-reducible countdown and looping animation cursor are distinct clocks.
  The existing rollback source cursor reproduces all 127 captured observations
  and the 99-to-0 wrap exactly, while the generic cyclic-frame verifier owns the
  reusable source theorem.

## Completed and verified: Falcon Dive and common FallSpecial DAT/HSD ECB ownership

- [x] Expand the existing parent-closed HSD profile instead of adding a second
  animation parser or move-specific pose tables. This slice first brought the
  profile to 17 motions, 1,204 tracks, and 10,106 keys; the shield-break slice
  above supersedes those generated counts without changing this ownership.
- [x] Route Falcon Dive ground/air start, catch, throw, and common
  FallSpecial neutral/forward/back through one allocation-free matrix-to-ECB
  evaluator. Raptor and Dive share source-clock dispatch, entry-pose ownership,
  bottom-lock application, TransN subtraction, and four-point ECB conversion.
- [x] Reproduce `ftCo_Fall_Anim_Inner` plus `ftCo_800CC988`: a newly selected
  directional target is blended once when installed and again after its target
  skeleton advances; an unchanged target takes only the latter pass. Persist
  the one-bit switch fact because it changes collision geometry and rollback.
- [x] Qualify nine retained live captures / eight motions / 733 rows / 715
  unique source frames with maximum error two Q16 units. Three focused C cases
  cover FallSpecial entry, target switch, and stable continuation within 64
  Q16 units.
- [x] Refresh the natural `up_air_miss_natural` Dolphin route and replay the
  same inputs through production. All 165 compared frames pass with strict
  actions and velocities plus the existing 640-Q16 position envelope; the raw
  refresh capture is SHA-256
  `55ddf2eed8a8cd52d788075b62bca1e6d7be3a1a26ce89bec6e02b85475ea5b4`.
- [x] Remove the authored Raptor Boost, Falcon Dive, and FallSpecial ECB
  arrays. FallSpecial loop length now comes from the imported submotion catalog,
  leaving one source owner for both timing and geometry. The regenerated
  Falcon include was SHA-256
  `754a72159e5463752e382dd6a2a8e35657bab601b84c228ade5c540d30272a74`
  at that checkpoint; the later shield-break import now owns the current
  include SHA-256
  `11a8489bdafe06c8df42ab5c527411a7679fb240913076c9bdae3e28c4f1b238`
  and complete-source digest
  `280abf47cbc18b5802e1c98048c7830808541766dd6c646d31c34eb0b0d3eb64`.
- [x] Carry the new observable switch state through canonical reset, tick,
  snapshot, replay, inspection, and validation. State schema is 74, inspection
  schema is 56, save size is 1,747 bytes, and replay size is 42,519 bytes.
- [x] Pass the full WSL Release suite 40/40 in 5.67 seconds and strict native
  Windows MSVC suite 40/40 in 8.35 seconds, plus the direct HSD source theorem,
  deterministic replay/snapshot tests, web/native smoke, and reproducible
  Falcon/common-data import checks.

## Completed: exact fractional Walk/Run ECB ownership

- [x] Re-sweep pinned/current `doldecomp/melee`, current HSDLib, and the
  existing HSD evaluator before changing production. Pinned/current
  `ftanim.c` and `mpcoll.c` are unchanged; no maintained external tool ports
  Melee's complete local-SRT blend state into a deterministic fixed-point
  runtime.
- [x] Extend the existing surface probe with the six `ECBSource` JObj pointers,
  their parent-closed 25-joint local SRT/world matrices, and the corresponding
  `FighterBone` flags. A four-worker live pack captures all four gaits in
  7.48 seconds warm; the focused one-worker WalkMiddle pack takes 4.66 seconds
  warm including launch/menu setup.
- [x] Isolate and correct the former 0.02-0.16 source-unit discrepancy. The
  capture's Player 1 uses costume ID 1 (`PlCaGy.dat`), while the evaluator used
  neutral `PlCaNr.dat`. The live leaf translations 25/47 exactly match the gray
  costume bind model; `ftAnim_8006FA58`/`ftAnim_8006FB88` copy that active
  costume SRT before attaching target tracks. No persistent-channel state is
  needed after blend completion.
- [x] Generate the six ECB source-joint selectors from the same 25-joint
  parent-closed catalog used by hurt geometry, reproduce grounded
  `mpColl_LoadECB_JObj` including its strict 10-unit symmetry predicate, and
  enable WalkSlow/Middle/Fast and Run production routing. Two independent live
  captures qualify 262 post-blend poses / 2,882 hurt capsules at maxima of one
  Q16 ECB unit and two Q16 hurt units; eight fractional/looped C observations
  protect both consumers.
- [x] Add the smallest shared representation of Melee's six-frame local-SRT
  transition recurrence, including a gait change begun before the previous
  blend completes, then qualify transition rows twice. The source half is now
  pinned: two independent 193-row captures verify 54 moving-target updates /
  1,296 joint recurrences within `1.2330335e-6` local and `2.20395109e-7`
  quaternion units, with 158 converged rows protected by semantic SHA-256
  `cd3aba1802a0b749e2b677e720eadb4c97b254574b548f184be529589ef16f1d`.
  Production now stores only 19 canonicalized Q15 quaternion xyz triples, six
  Q16 translation triples, and one Q16 progress value per player. A 27-row C
  oracle replays every captured moving-target update, including nested gait
  changes and accumulated compact re-quantization, with maxima of 8 Q15
  quaternion units and 4 Q16 translation units. Five checkpoint-equivalent
  production ticks now bind the real `pf_sim_tick` path to Wait-to-Walk,
  WalkSlow-to-Middle, nested WalkSlow-to-Middle, WalkMiddle-to-Fast, and
  Dash-to-Run capture rows. This exposed and fixed Melee's pre-IASA ordering:
  the old animation and any active old blend advance before the new action is
  selected. Production geometry now matches those entries within 4 Q15
  rotation and 4 Q16 translation units. Hurt geometry, inspection, and wall
  ECB consume the same reconstructed pose. No persistent per-channel rollback
  state is used for post-blend poses.
- [x] Run the full Windows/WSL/sanitizer/stored/replay/benchmark gates. Windows
  Release passes 40/40, WSL Release passes 40/40, and WSL ASan/UBSan passes
  26/26 after the production-transition correction. The complete 21-domain /
  117-case stored-plus-replay gate takes 1.018 seconds on Windows and 0.686
  seconds in WSL. The canonical tuple is now state 71 / save 66 / `PFSAVE60`,
  with a 1,567-byte payload and 1,707-byte checkpoint. A clean exact-parent
  WSL benchmark compares all ten available scenarios as compatible, with zero
  suspected or confirmed regressions. It uses the installed GCC 13.4.0 under
  the explicit unpinned-compiler override because the pinned 13.3.x compiler
  is unavailable on this host.

## In progress: broader action-specific ECB ownership

- [x] Replace Raptor Boost's route-captured 45-value aerial-hit bottom table
  with the shared DAT/HSD evaluator for all four ground/air start/hit motions.
  The evaluator subtracts the live TransN reference, applies Melee's grounded
  and airborne bottom policies, retains the four-update airborne bottom lock,
  and preserves previous-action pose ownership on frame-zero hit transitions.
- [x] Qualify the source evaluator against five raw Dolphin captures: 423
  observations spanning 411 complete action frames agree within one Q16 unit.
  Four direct C cases accept at most 32 Q16 units from deterministic fixed-point
  evaluation, and the complete 657-frame production Raptor suite passes.
- [ ] Continue the remaining action-specific ECB audit without introducing
  route tables when imported DAT/HSD data and the common evaluator can express
  the source behavior directly.

## Implemented and verified: decomp differential and web-startup cleanup

The 2026-08-10 source audit is now followed by actual generation, native/WSL,
stored-oracle, live Dolphin, Emscripten, and browser validation. The audit still
does not imply whole-game equivalence: only the registered live and stored
domains below are empirically qualified.

- [x] Replace authored or collapsed common behavior with source-routed mash,
  capture, grab/release, throw, shield-break, damage, hitlag, hitstun, meteor-
  cancel, rebirth, teeter, rebound/clank, dash/walk, jump, escape, and input-
  priority semantics wherever the current state model can express the decomp.
- [x] Add Falcon's missing Jab 3 and rapid-jab lifecycle, angled forward-tilt
  and forward-smash actions, exact smash-charge release clock, down-tilt repeat,
  pummel hitlag, throw weight scaling, stale scaling, C-stick defensive edges,
  and special/common collision transitions without adding authored frame data.
- [x] Preserve exact source identities for `Pass` (raw submotion 209),
  `Ottotto`/`OttottoWait` (210/211), `CatchCut`/`CaptureCut` (246/257), and the
  shield-break family (286-291). Keep raw DAT submotion identity separate from
  action-state identity.
- [x] Extend canonical rollback state only for callback-consumed history and
  continuation: mash directions, previous C-stick samples, horizontal input
  age/direction, rebound duration, jab-chain state, damage-jump buffering, and
  match KO/fall counters. The active compatibility tuple is content 77,
  fighter 69, state 71, save format 66, magic `PFSAVE60`, 1,567-byte payload,
  and 1,707-byte checkpoint.
- [x] Repeat the source-to-production callback audit across Wait, Dash/Run,
  Damage/DamageFall, Guard, Ottotto, Catch/Throw, Rebirth, Rebound, common
  ledge eligibility, and all four Falcon specials. No further discrepancy was
  found that is decidable from the currently represented decomp/data paths.

Remaining work is evidence/model work rather than a known code-only fix:

- [x] Import exact executable pose/hit geometry for Jab 3, rapid jab, every
  forward-tilt angle, and Falcon's real high/mid/low forward-smash variants.
  The two absent forward-smash slots remain source-absent. Two independent
  2,974-row headless/unlimited captures reproduce the same semantic payload.
- [x] Import exact pose/hit geometry and lifecycle for pummel/CaptureDamage.
  Two independent 3,142-row captures reproduce the same semantic payload; the
  generic stored domain hashes all 79 poses and exact active-frame hit/miss
  geometry.
- [x] Import and physically qualify exact pose geometry for the complete
  158-frame crouch-wait loop and both distinct 60-frame taunt orientations.
- [x] Implement Melee's velocity-driven WalkSlow/Middle/Fast and Run animation
  cursor, gait selection, and walk-phase remapping. The generic clock oracle
  protects three cases / 111 source samples through the shared C runner.
- [x] Bind WalkSlow, WalkMiddle, WalkFast, and Run fractional hurt geometry to source DAT
  tracks through the shared deterministic Q16 HSD evaluator. The live source
  comparator passes both independent 131-sample / 1,441-capsule captures with
  a maximum 2-Q16 delta; eight stored observations protect the production C path. WalkFast is reached
  naturally by entering Walk below the dash threshold before raising the stick
  above Falcon's fast-gait velocity boundary.
- [x] Import all 158 CrouchWait four-point ECB poses, retain its ordinary
  one-frame source animation clock through canonical tick/snapshot state, and
  select each pose in O(1) from the existing cursor. The independent
  160-row live repeat reproduces semantic SHA
  `ba47ef2736a5677d1909262a20f32991b7c2515407fae26626d5869b95edd265`.
- [x] Remove signed-overflow dependence from shared Q16 collision ratios. The
  fast path is unchanged; only values that cannot safely multiply by 65,536
  use an exact fixed 16-step unsigned divide. This restores bit-identical
  Hyrule contact selection under ASan/UBSan without changing native output.
- [x] Reproduce WalkSlow/Middle/Fast and Run ECBs through the exact HSD
  six-frame blend and the decomp's 10-unit symmetry branch. The shared compact
  evaluator and fixed-point blend recurrence now drive production collision;
  five checkpoint-equivalent ticks agree within 4 Q15 rotation and 4 Q16
  translation units, including Wait-to-Walk, nested gait change, and
  Dash-to-Run boundaries.
- [x] Extend the live hitbox/hurtbox probe with Fighter `cur_anim_frame`
  (`+0x894`) and `frame_speed_mul` (`+0x89C`). A fresh seven-checkpoint,
  423-row capture proves the decomp callback order and exposes fractional
  Walk/Run clocks for the implementation comparator.
- [x] Import shield-break DownU/DownD selection from the terminal ShieldBreakFly
  HipN matrix. Falcon's source component is `-3921` in Q16, so production enters
  DownD then StandD. Two independent 500-row natural shield-depletion captures
  reproduce `ShieldBreakFly -> DownD -> StandD -> Teeter` with exact action
  counts and IDs. Import the complete 42-frame animated ECB so landing occurs
  on the source update; retain the one-frame landing vertical velocity; preserve
  hit-break reset health versus passive-depletion zero health; and run global
  regeneration through Fly/Down/Stand plus Furafura's per-frame reset.
- [ ] Represent dynamic rebirth targets/companion coordination, full stage and
  item-kind behavior, aerial item/tether callbacks, and tournament entry/rule
  choreography before claiming those wider SSBM domains.
- [x] Remove the duplicated simulation scenarios from production browser
  startup. `m4_playtest.c` is reduced from roughly 14,000 to 1,100 lines and
  `pf_web_m4_playtest_install` now receives four real configuration values
  instead of four values plus 58 probe statuses. Native simulation suites own
  simulation fidelity; the web gate retains adapter ABI, controller polling,
  mapping, UI, Wasm-load, and real browser interaction checks.
- [x] Remove the remaining host-compiled gameplay duplication from
  `tests/web/test_m4_playtest.c`. The adapter test is reduced from 1,337 to 397
  lines and now checks only startup/view packing, exported input endpoints,
  independent trigger edges, hit-geometry presentation, duel/team setup, and
  four-player routing. Falcon move lifecycles, tactics, items, and combat-event
  semantics remain solely in native simulation, replay, and SSBM-oracle lanes.
  The shell gate is reduced from 421 to 150 lines by deleting action-name and
  simulation-source greps; it retains syntax checks and compact browser-only
  Gamepad, WebUSB, controller, visual-cue, and endpoint contract markers.
- [x] Correct the imported initial-dash frame clock so an A press after source
  Dash frame 4 enters DashAttack, keep ground-damage animation advancing after
  hitstun unlocks, and remove Falcon's incorrect six-frame delayed-double-jump
  projection. The pinned 64-row ground-knockback capture passes all 15 compared
  samples, and both 520-row natural-landing captures pass the corrected ordinary
  JumpAerial projection.
- [x] Pass Windows Release 36/36 in 1.49 seconds and WSL Release 38/38 in 0.92
  seconds after the DownWait/getup ECB import. The latest stored gate covers all
  21 domains / 117 cases plus replay in 0.932 seconds on Windows and 1.723
  seconds in WSL. WSL ASan/UBSan passes 25/25 in 9.22 seconds. The rebuilt
  Emscripten target and real in-app-browser
  startup/control/console smoke pass; starting a local match advanced the live
  simulation to tick 50 without console warnings or errors.

## Completed and verified

### Equivalence architecture

- [x] Pin owner GALE01 NTSC-U revision 2, decomp revision, Dolphin/ExiAI,
  libmelee, capture protocol, and source/production digests.
- [x] Replace per-experiment Dolphin GUI launches with one headless,
  null-renderer, unlimited-speed process per compatible packed run.
- [x] Restore checkpoint-isolated microcases without reconnecting the Slippi
  observer.
- [x] Batch fighter memory reads and cache unique bone matrices.
- [x] Prune serialization to manifest-declared observations while retaining all
  required command ticks.
- [x] Add character-independent stored-oracle registry, generator, affected-file
  selector, C runner, digest checks, replay gate, diagnostics, and time budget.
- [x] Keep Falcon-specific stored-oracle code to data bindings and thin
  production adapters.

Relevant commits on `agent/m4-combat-vertical-slice`:

- `dbf12f0` - checkpointed Dolphin foundation.
- `5c68258` - generic manifest-selected stored SSBM oracle.
- `b113dbb` - manifest-controlled live observation pruning and warm-budget gate.
- `b3edb14` - qualified open-air damage response, numeric stored oracle, and
  complete `PlCo.dat` import.

### Falcon data and qualified behavior

- [x] Import the complete 318-slot Falcon submotion catalog, all source action
  scripts, all translated root tracks, common character attributes, special
  attributes, stale-move table, and qualified attack/hurt geometry.
- [x] Qualify the currently registered `falcon-common-hurt` domain, including
  complete pose coverage and physical hit/miss discriminators.
- [x] Preserve deterministic replay/snapshot behavior for completed slices on
  Windows and WSL.
- [x] Import all 11 standing-turn and 22 running-turnaround hurt poses from a
  two-case live/control pack; retain source submotion in snapshots and register
  the 33-pose production accessor as its own stored-equivalence domain.
- [x] Preserve TurnRun's one-update gameplay-facing/display-facing split in
  world hurt collision and inspection by deriving it from existing canonical
  state; verify the adjacent phases with reusable stored pose-facing cases.

## Completed locally: native Battlefield playtest client

- [x] Replace the static M1 render-packet window with a direct public-API M4
  simulation session using the source-qualified Battlefield content
  constructor, allocation-query contract, deterministic reset, and fixed
  60 Hz stepping. The smoke-only path and its existing CI contract remain
  unchanged.
- [x] Add one allocation-free public stage-geometry view and render all 23
  imported Battlefield floor, ceiling, and wall lines. Render the exact blast
  bounds in an inset rather than inventing a second stage representation.
- [x] Render both fighters plus facing, damage, stocks, action/frame, crouch
  size/color cue, full/light shield bounds, active attack bounds, and a
  switchable collision inspector. The inspection schema exposes the same 11
  transformed source hurt capsules used by combat, so presentation has no
  duplicate pose transform. Add pause, single-step, reset, and resizable
  high-DPI window controls.
- [x] Add allocation-safe match setup through `pf_sim_default_config`, memory
  query, reinitialization, and reset: F2 atomically switches the only supported
  match pairs (2P Duel / 4P Teams), while F3 cycles 1-8 stocks. Screenshot QA
  confirms all four imported Battlefield spawn points and source hurt poses.
- [x] Reuse SDL's semantic Gamepad API for mapped devices and retain a narrow
  raw-joystick adapter for Mayflash PC mode. The real four-port adapter sample
  established the SDL-specific layout: main stick axes 0/1, C-stick 2/3, and
  L/R 4/5 at `-32768` neutral. This is intentionally separate from the Web
  Gamepad layout.
- [x] Bind Mayflash ports by first real input activity, so neutral empty ports
  cannot consume P1/P2. Once claimed, a port remains stable through neutral
  frames. Keyboard input remains an additive fallback for both players.
- [x] Pass strict Windows MSVC `/W4 /WX`, focused movement and combat suites,
  and the native SDL smoke. Direct window capture confirms the exact stage,
  both source spawn supports, HUD, blast inset, and stable idle state with the
  four-port adapter connected.
- [ ] Complete one hands-on GameCube controller pass covering both sticks,
  A/B/X/Y/Z, independent analog L/R, C-stick roll/attack, jump, shield,
  airdodge, pause/reset, and the crouch cue before marking this product gate
  complete.

## Completed locally: ordinary airborne submotion clock

- [x] Sweep pinned and current `doldecomp/melee` Fall/animation callbacks and
  Falcon's complete imported submotion catalog before changing runtime state.
  The relevant source files are unchanged at current upstream head, and no
  maintained reusable implementation was found.
- [x] Import `ftCommonData.x78` instead of authoring the backward-jump motion
  threshold. Preserve one compact source submotion per fighter while retaining
  the allocation-free public `AIRBORNE` action.
- [x] Advance JumpF/JumpB through their imported animation lengths into Fall,
  JumpAerialF/JumpAerialB into FallAerial, and wrap all ordinary Fall families
  at their imported eight-frame length. `action_ticks` is the exact zero-based
  source animation phase.
- [x] Extend canonical snapshot/hash/replay state by eight fixed bytes, reject
  invalid action/submotion/clock combinations, and migrate to state schema 64,
  save format 60, magic `PFSAVE54`, a 695-byte payload, and an 835-byte save.
  This is the verified historical checkpoint; the newer unverified static
  slice is recorded separately above.
- [x] Qualify the eight-frame Fall loop in the registered Hyrule theorem and
  compare the existing 1,250-frame Dolphin aerial-iasa capture. All 350
  JumpAerial/FallAerial action-frame samples pass strictly. WSL and Windows
  Release pass 27/27; WSL ASan/UBSan passes 20/20; native and Wasm replay are
  byte-identical; the browser adapter and Windows Chrome DOM/Wasm smoke pass.
- [x] Add the coalesced-public-action/source-submotion rule to the reusable
  character-importer skill and validate the skill package.

## Completed locally: deterministic prone response and accelerated live oracle

### Prior art and rejected paths

- [x] Sweep the pinned and current `doldecomp/melee` sources plus current
  libmelee before extending the oracle. No newer implementation replaces the
  existing source/action route.
- [x] Identify the cross-boot instability: `gmmain.c` initializes the shared
  HSD random seed from `OSGetTick()`, while high-knockback damage selection
  consumes `HSD_Randf()` and can choose `DamageFlyRoll`.
- [x] Make the source RNG seed an explicit manifest-owned checkpoint-pack
  event-boundary value, validate the live seed pointer, and read back the
  single pre-hit write while Dolphin is blocked on the corresponding input
  frame. This keeps source randomness visible and reproducible without
  resetting the stream on every emulated frame.
- [x] Prove two fresh independent Dolphin boots produce the same 2,370 rows,
  940 semantic samples, and
  `fc91d42660ac0a8df8f0715b183b2ec97bccfe2ee0279491cadf915e64044438`
  digest with the pinned seed. One transient checkpoint-host crash was retried
  successfully and remains a harness reliability item, not accepted output.
- [x] Reject the first combined 14-case pack despite correct route generation:
  response-only serialization reduced output to 1,515 rows, but replaying all
  physical setups serially still took 34.77 seconds end to end.
- [x] Reject the arbitrary three-slot branch-checkpoint experiment. Restoring
  at live action branches desynchronized host/game boundaries and was removed
  from the capture tool and reproducible Dolphin patch.
- [x] Sweep prior art before replacing it: libmelee supports independent
  Slippi ports, and multi-environment emulator workloads already use isolated
  Dolphin processes. The accepted design therefore shards real physical routes
  instead of fabricating source state.

### Accepted implementation and evidence

- [x] Add frame-safe `BATCH ON` pipe input. Queued FLUSH-delimited samples are
  consumed once per emulated frame; background controller polls are gated by
  ExiAI's `g_needInputForFrame` boundary.
- [x] Run four headless/null/unlimited Dolphin shards concurrently on distinct
  Slippi ports. Unique hardlinked process names let Dolphin Memory Engine hook
  the intended shard, and a generic merger restores manifest case order while
  checking disc, emulator, library, stage-collision, and probe provenance.
- [x] Qualify all 14 physical cases and 1,515 rows twice with semantic digest
  `db317711cb1a5b2c877d4dc8dd57e1ef38c31edd93638dd1d05e272b6d46cd8d`.
  A clean patch rebuild plus live-and-simulation gate passed in 9.812 seconds,
  below its 18-second guardrail. The first uncached concurrent run also exposed
  and fixed a process-shared SHA-256 cache temp-file race.
- [x] Capture and qualify Falcon's opposite prone orientation, including
  timeout, buffered getup attack, C-stick roll, and main-stick roll routes.
- [x] Implement the exact decomp distinction between semantic posture and roll
  motion: `ftCo_Down_CheckInput` chooses U/D roll motion from `DownWaitU`, so a
  roll selected directly from terminal `DownBoundU` intentionally uses the D
  root track and invulnerability table while retaining Back posture semantics.
- [x] Preserve this distinction in canonical snapshots without increasing the
  wire size by using previously unused bits in the prone/tech byte.
- [x] Pin the 14-case stored production trace digest
  `e4e6554506bf01ba299628a205dbe911db967e257812f3142225bd1afc606256`;
  the focused stored lane passes 168 selected production samples.
- [x] Pass all six stored domains and deterministic replay: 50 cases in
  606.209 ms on Windows and 672.759 ms on WSL.
- [x] Pass the full Windows Release suite 25/25 in 1.93 seconds, full WSL
  Release suite 25/25 in 1.44 seconds, and WSL ASan/UBSan suite 18/18 in
  12.04 seconds. The Windows SDL smoke also builds cleanly with the unnecessary
  `SDL_main` wrapper removed from the console executable.
- [x] Import and qualify `DownBound` ECB pose-grounding, including its observed
  4-contact / 18-no-contact / 4-contact sequence, without treating the action
  as ordinary airborne fall. Both orientations now consume one imported
  26-frame bit mask each, retain the source floor line and ground physics
  during the no-contact interval, and discard that retained support if
  airborne damage replaces DownBound.

Current DownBound prior-art/source sweep:

- The pinned decomp and current `doldecomp/melee` head have no relevant change
  in `ftCo_DownBound.c`, `ft_081B.c`, or `mpcoll.c`; current forks and issues
  expose no maintained reusable DownBound/ECB importer.
- `ftCo_DownBound_Phys` keeps ground-friction/root physics, while
  `ftCo_DownBound_Coll` runs `ft_80082708` over the JObj-derived ECB. The live
  action remains DownBound while the reported contact flag changes, so these
  frames must not be converted into ordinary airborne fall.
- Existing live memory evidence contains both Falcon orientations. Their ECB
  shapes differ, but both exact 26-frame contact schedules are frames 1-4 on,
  5-22 off, and 23-26 on. The canonical schedule-pair digest is
  `6c8d97ff1076075616ed06f88c742528eff9c2fb18ab9f2cce09ba895147e556`.
- Fresh identical-input qualification passes all 804 flat-floor rows / 232
  response samples with strict ECB contact comparison. The accelerated pack's
  warm capture took 3.032 seconds and the complete run took 6.065 seconds.
- The pinned ISO/DAT/JSON toolchain regenerates
  `m4_falcon_ntsc102_frame_data.inc` byte-identically at
  `d6e2700a293450bf8f8e5d075881e6284cf09bb56a41e68590a36d779f72004e`;
  its expanded complete-source digest is
  `7b34f2eb4e8edf0c491ea410c27b9790f2127d90a560865c56ebc68bea98c170`.
- The floor-response production trace is
  `bbf01b67f222d78e915f5077eecd9a45282f77e7d32efb4cf7bf8d79b513eb2f`;
  the two-orientation prone trace remains
  `dc416f87ccd228a045c61e87350af2619bad244e6a83dc0708502b414886fefb`.
- Final gates pass native Windows Release 25/25 in 3.90 seconds, WSL Release
  25/25 in 1.72 seconds, WSL ASan/UBSan 18/18 in 11.91 seconds, and all six
  stored domains / 50 cases plus replay in 0.744 seconds on Windows.

## Completed and verified: reusable paired-fighter player-push equivalence

- [x] Re-audit the existing 540-frame Final Destination Falcon-versus-Falcon
  route before writing new behavior. It already qualifies both horizontal
  directions and both controller ports, including strict action, facing,
  grounded state, and velocity plus the documented one-nudge Q16.16 position
  allowance.
- [x] Recheck pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` against current
  `doldecomp/melee` master `013091add6d46d2d809d163371deab97ab5e37eb`.
  `ftcommon.c` and `fighter.c` are unchanged for this route; Project Slippi
  and libmelee provide transport/observation but no maintained reusable
  player-push equivalence domain.
- [x] Replace the legacy monolithic comparator-only proof with manifest-owned,
  checkpoint-isolated paired cases and a shared paired numeric-trace runner.
- [x] Pin fresh live and production semantic digests, register the domain in
  the sub-two-second stored gate, and rerun Windows/WSL/replay validation.
- [x] Generalize numeric traces to one or two frame-major observation lanes and
  manifest-selected serialized fields. Existing one-lane byte streams and all
  five numeric production digests remain unchanged; repeated input phases are
  expanded only by the offline generator with compile-time lane counts.
- [x] Reproduce the 48-row / 96-lane source trace on three fresh Dolphin boots.
  Source digest
  `3c6ade86d516474c60b7559690b3b858f2b7a66b41982859e4f81df70a7c73f5`
  and production digest
  `079a34868db4fff30719d7a784d7bd102aab7a81acd189ad6293c65a9056bc7a`
  are pinned. Action, facing, and grounded state are strict; velocity allows 32
  Q16 units and position retains the reviewed 2,692-Q16 one-nudge envelope.
- [x] Run the final live pack in 0.090 seconds warm / 3.011 seconds end to end,
  the seven-domain stored gate in 0.802 seconds on Windows and 0.716 seconds in
  WSL, Windows Release 26/26 in 7.31 seconds, WSL Release 26/26 in 1.52
  seconds, and WSL ASan/UBSan 19/19 in 12.27 seconds.
- [x] Extend and validate the `ssbm-character-importer` skill with the reusable
  paired-lane/field-mask pattern.

## Completed and verified: imported Hyrule slope and ordinary ledge response

- [x] Sweep the pinned/current `doldecomp/melee` stage-collision, DownBound,
  damage-flight, and ledge routes before adding project machinery. No maintained
  reusable importer or exact-response harness covers this slice; ExiAI remains
  the qualified transport.
- [x] Import Hyrule MapCollData lines 34-37 into one immutable runtime catalog.
  Raw vertices are converted from joint-local to runtime world space, while the
  semantic source digest
  `4a0dd57bb8d9532589d3ecd129213d3a0876538a2dc7f733eca6c1e73c04db9c`
  pins topology independently of generated-C formatting.
- [x] Import Falcon's 24-frame DamageFly ECB-bottom track and source ledge-snap
  attributes. Production now projects landing knockback onto the imported slope
  tangent, evaluates DownBound contact before decay, retains support only until
  current and previous roots leave the endpoint, clears hitstun on the exact
  collision-driven Fall boundary, and performs ordinary ledge reach from source
  root/ECB coordinates.
- [x] Qualify two checkpoint-isolated physical cases: line-34 forward getup roll
  and line-36-to-line-37 natural hit departure through exact ordinary ledge
  catch. The live source digest is
  `8c62ce678732b38d157f1e3cee2409b0da22835bc63c906c41e765ce1a879a6d`;
  the production digest is
  `4dce7db6baa8a11fb90b77438ef5e423b500e5e5a3f7a4da835bfc979d6f0167`.
  Two fresh Dolphin boots reproduced the source digest. The final live gate
  measured 1.234 seconds warm and 4.859 seconds including process lifecycle.
- [x] Generalize stored numeric cases with an optional per-case field mask.
  Inherited masks emit an explicit zero initializer so MSVC and GCC
  `-Werror=missing-field-initializers` agree without runtime branching.
- [x] Requalify the damage and floor domains after correcting DamageFly entry
  grounding and landing ordering. Their production digests are
  `53471e3af8475959868a5f64361200151bc9fd9b640dbb5b3d66e4cd5031db4b`
  and `0596ba59ba2f03076b8b6828bae662eda6a7861ff8222af8788b47738aeece16`.
- [x] Repin deterministic replay only after the live-qualified production
  changes: 41,599 bytes, corpus
  `0d1c16c1e231d29c89a49d193f6b10deb081297821d5448239307cae4d33f4ad`,
  final state
  `6c648e4463b070ad4b7e3b013ea620e21463b281fe39b00980cf0cbf558bfcd5`,
  and events
  `0cf114479e7cec86ebe0b89b08fd6eabc74209d99ed053fb92b397d26d6eab8e`.
- [x] Pass the eight-domain/54-case stored gate in 0.804 seconds on Windows and
  0.778 seconds in WSL; Windows and WSL Release pass 27/27, WSL ASan/UBSan
  passes 20/20, and the cross-platform verifier soak digest is
  `7f584c16f3d23773`.

## Completed locally: common ledge down-input rejection boundary

- [x] Re-sweep current and pinned `doldecomp/melee`, libmelee, Slippi, and
  Dolphin prior art. Current decomp changes do not touch `ftcliffcommon.c`,
  `mpcoll.c`, or the common-data layout; no maintained executable replacement
  covers this branch.
- [x] Import `PlCo.dat` common-data word `x480` (`0.6600000262260437`) as the
  immutable Q15 ledge-grab down-axis threshold and consume it in the shared
  ordinary-ledge predicate. Requested controller values are kept separate from
  the quantized values observed by Melee.
- [x] Add adjacent live controls: requested `-21400` is observed as source
  `-0.65` and catches, while requested `-21626` is observed as source
  `-0.6625000238` and remains in Fall. Both routes reuse the existing Hyrule
  damage/DownBound/endpoint fixture; no duplicate setup or runtime branch was
  added.
- [x] Reconcile the DownBound endpoint callback with its imported contact mask:
  the first contactless ECB frame consumes the current root, while later
  contactless frames retain the prior floor root. LedgeCatch entry now clears
  all knockback channels on the source transition.
- [x] Expand the checkpoint pack from 180 to 290 rows and the stored projection
  from two to four cases / 220 samples. Two fresh warm captures complete in
  2.716 and 2.493 seconds and reproduce semantic source SHA-256
  `9df8c72fca21359281d7d89391a9c363e08e6cf5c06db8873868e10521f27b49`;
  the reviewed production trace SHA-256 is
  `73f3dae4bf726aedd1e2ab37911818faa9b3fff4d1a19ed2a92a41148f142f5d`.
- [x] Express position allowance as base Q16 error plus at most the already
  qualified velocity error per integrated tick. This bounds fixed-point drift
  without widening a flat magic tolerance.
- [x] Reproduce the intentional seeded match-soak identity
  `52e8cab76719e97c` three times each on Windows and WSL before repinning it;
  all six runs report 8 matches, 2,848 ticks, and identical event counts.
- [x] Pass Windows GCC Release 28/28 in 4.15 seconds, WSL Release 30/30 in
  1.99 seconds, WSL ASan/UBSan 23/23 in 14.16 seconds, and the eleven-domain /
  79-case stored gate plus replay in 1.277/1.370 seconds on Windows/WSL.
  Regeneration, documentation, and importer-skill validation pass. The local
  pinned MSVC lane is unavailable because Visual Studio `vswhere.exe` is not
  installed; remote CI retains that compiler gate.

## Completed and verified: Falcon Dive action-specific ledge acquisition and aerial-catch geometry

- [x] Re-sweep pinned/current `doldecomp/melee`. The relevant
  `ftCa_SpecialHi.c`, `ft_081B.c`, `ftcliffcommon.c`, and `mpcoll.c` paths are
  unchanged at current head `6ddd74ecbb755df25b32f137b5f7b7f6d7005e91`.
- [x] Preserve the collision callback's direction policy as one zero-cost
  shared ledge-probe value: Falcon Dive start passes direction `0` and can
  acquire either endpoint after its imported command-variable gate; ordinary
  airborne/fall states remain facing-only. Falcon Dive throw no longer inherits
  a ledge query that its source callback never makes.
- [x] Apply the common catch transition's inward facing from the selected ledge
  endpoint. A deterministic physical route turns Falcon outward during Dive,
  catches the ledge behind him, and asserts inward facing plus zero velocity.
- [x] Preserve the existing 63-frame facing-toward Dolphin route: source still
  catches on frame 64, enters `EdgeHang` on frame 71, and production passes all
  63 comparable frames with the 640-Q16 envelope.
- [x] Capture and pin the facing-away route in headless/null/unlimited Dolphin.
  Source retains outward facing from Dive frame 13 through frame 63, catches
  on frame 64, turns inward, and enters `EdgeHang` on frame 71. All 63
  production samples pass within the 640-Q16 envelope; raw SHA-256 is
  `026faf91c3582aa5e41c5d95ba757904ec7ef7865a049994ce169f70a6157009`.
- [x] Repair the pre-existing aerial-catch validation lane. Bisect identifies
  `821fde3` as the first red commit: it correctly changed imported geometry from
  feet-origin to Melee reference-joint origin, while the source route pins both
  fighters at `y=500` and the native runner follows a different natural-jump
  trajectory. The repaired lane imports collision-authoritative JumpF frames
  9-20, replays exact source-relative placements, and uses a shared stored
  sphere-versus-pose predicate. Its pinned hit and translated miss margins are
  `+4.645228676` and `-1.151280430`; no production-origin rollback or
  character-specific collision routine was introduced.

## Completed and verified: distinct CliffCatch and CliffWait entry

- [x] Reuse the already-qualified Hyrule route and current/pinned decomp sweep;
  no redundant Dolphin boot or new character-specific capture lane was needed.
- [x] Import live-qualified absolute frame-one catch/wait roots, reuse decoded
  `TransN` deltas, preserve seven displayed catch frames, and enter wait only
  on source animation completion.
- [x] Expand the live comparison to 110 production samples with source digest
  `0b23132b7a217ff173397faf8ac9e59169092c99095b4b4e3fbd885526b7a3f3`
  and production digest
  `9c562426f42c4b01b08a7bbea9c667f56661a2787d107870a14208f326ccd94e`.
- [x] Validate catch in snapshot/hash and exclusive ledge ownership, and count
  its seven elapsed invulnerability frames without restarting at wait.
- [x] Pass WSL Release 27/27, Windows MSVC Release 27/27, WSL ASan/UBSan 20/20,
  the 0.605/0.805-second WSL/Windows stored gates, Emscripten rebuild,
  browser adapter, native/Wasm replay identity, and Windows Chrome DOM smoke.
- [x] Update provenance, fidelity audit, roadmap, and importer skill.

Remaining scope is explicit: ordinary Fall animation-clock equivalence,
later wait/ledge-option behavior and geometry, post-wall-contact absolute X,
broader stage topology, the other incomplete fidelity-audit rows, and the native
Battlefield frontend.

Primary sources:

- Owner `PlCo.dat` SHA-256
  `63841336337eb5a7366b06ccc60ea4bd37c3604ab56e19939d78b9aa9cdd234c`.
- `doldecomp/melee` revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7`.
- `ftCo_Damage.c` for SDI, ASDI, and DI operation order.
- `fighter.c` for the later separate knockback-velocity decay channel.

Implemented and pushed in `b3edb14`:

- [x] Strict reusable HSD archive root/relocation validation in
  `tools/ssbm_dat.py`.
- [x] Separate character-independent `PlCo.dat` generator preserving all 518
  raw `ftCommonData` words plus typed damage-response fields.
- [x] Source-derived 18-degree DI limit, radial 0.7 SDI/ASDI threshold,
  four-frame SDI window, analog SDI/ASDI distances, and shield multiplier.
- [x] Shared allocation-free fixed-point DI and analog-displacement primitives.
- [x] C-stick ASDI priority over the main stick.
- [x] Rotate DI in Melee coordinate units before converting back through the
  project's anisotropic X/Y world scale.
- [x] Add an executable source-data check for all 518 words plus radial
  threshold, analog displacement, and squared-projection DI discriminators;
  focused Windows combat test passes with `ssbm_damage=1`.
- [x] Add source-derived discriminating cases for squared DI projection,
  threshold-adjacent radial SDI, analog displacement, and C-stick ASDI.
- [x] Capture and source-qualify a robust checkpointed live Falcon damage
  route.
- [x] Replay the same six input routes in the simulation and compare every
  hitlag/launch position, self velocity, knockback velocity, and timer against
  the live trace within a 0.001 Melee-unit Q16.16 envelope.
- [x] Keep the six-case live pack to 138 rows and a 2-second warm budget;
  measured warm capture is 0.671 seconds and its stable observation digest is
  `51402cd3605ba2761e3c11ed6baab74eb1b7ab22136822507b39d0a00cc40d95`.
- [x] Register the damage-response domain in the generic stored-equivalence
  lane.
- [x] Regenerate intentional replay/content hashes only after live
  qualification.
- [x] Run both stored-domain suites: Windows 205.990 ms and WSL 285.842 ms.
- [x] Complete the full affected Windows and WSL suites: Windows 21/21 in
  0.45 seconds and WSL Ubuntu 20/20 in 1.94 seconds. The verifier's new stable
  digest is `07cc4f4247d83066`; the synthetic browser ladder fixture explicitly
  retains its authored decay instead of inheriting imported Falcon data.
- [x] Pass WSL ASan/UBSan 16/16 after the final DI boundary review.
- [x] Commit and push the qualified slice to PR #3 as `b3edb14`.

Implemented and cross-platform verified locally after `b3edb14`:

- [x] Pin the decomp routes for grounded damage classification, Sakurai-angle
  handling, `xF0_ground_kb_vel`, friction decay, flat tangent projection, and
  the animation-plus-hitstun release boundary.
- [x] Capture a stable 64-row live late-DashAttack route in 2.801 seconds total;
  checkpoint restore is 0.011 seconds and the warm packed case is 0.128
  seconds.
- [x] Import the relevant `PlCo.dat` thresholds and ground-knockback constants
  rather than authoring replacements.
- [x] Add distinct `DamageLow1/2/3` runtime states and source submotion
  durations without expanding the canonical flat-stage snapshot payload.
- [x] Keep self velocity and ground knockback under separate deterministic
  state, including source-order friction decay and hitlag resume behavior.
- [x] Extend the generic numeric stored-oracle sample with action, resume
  action, grounded/tumble, damage, and ground-knockback fields; both prior
  domains continue through the same runner.
- [x] Register `falcon-common-ground-knockback` with one case and fifteen
  samples. Live-vs-sim comparison passes for every declared field; position is
  explicitly excluded because the chosen rows also exercise the separate
  player-pushbox domain.
- [x] Normalize only the attacker idle-loop phase that checkpoint restore does
  not freeze; two independent live captures now share observation digest
  `e08d7149e3f46d814d5c4a709e316cf3063208bb9673141effe6b1958f03fc79`.
- [x] Preserve the 807-byte canonical flat-stage save format by reconstructing
  `xF0` from the serialized flat-tangent `x8c` component; a mid-damage
  save/load continuation has exact state hashes and samples. State schema 61 /
  save format 57 fail closed on the new action semantics without adding bytes.
- [x] Clear the ground-only `xF0` scalar when a sliding fighter leaves a
  surface while retaining the projected airborne `x8c` velocity.
- [x] Review and repin the intentional deterministic replay identities after
  three identical runs; the root three-domain gate includes replay and passes
  27 cases in 0.305 seconds.
- [x] Pass the complete Windows release suite (21/21 in 1.13 seconds), WSL
  Ubuntu release suite (23/23 in 1.32 seconds), and WSL ASan/UBSan suite (16/16
  in 10.99 seconds).
- [x] Restore every standalone shell verifier's complete simulation source
  graph, then pass native/Wasm replay identity, the browser adapter verifier,
  and the full headless Chrome smoke with the repinned 81-event replay.
- [x] Commit and push this qualified slice to PR #3 as `9189aa0`.

## Completed locally: wall and ceiling damage response

Prior-art/source sweep completed before implementation:

- The pinned decomp routes are `ftCo_PassiveWall.c`,
  `ftCo_PassiveCeil.c`, `ftCo_FlyReflect.c`, `ftCo_Damage.c`, and the common
  collision callbacks in `ftcoll.c`; the public decomp and libmelee expose the
  same source actions `PassiveWall` 202, `PassiveWallJump` 203, and
  `PassiveCeil` 204.
- Falcon's already imported common-attribute words give passive-wall X speed
  0.5, wall-tech-jump X/Y 1.4/3.1, and passive-ceiling X speed 2.0. The current
  Q16.16 velocity constants are the exact project-unit conversions, so this
  slice must preserve them rather than replace them with new authored values.
- `PlCo.dat` words `x760=5` and `x764=14` feed the source wall-tech freeze and
  collision/invulnerability paths. Falcon's complete generated submotion
  catalog reports 26/40/26 endpoint counts for wall tech, wall-tech jump, and
  ceiling tech. Their exact observer-visible duration/order still requires a
  live route; the old 24/30 action durations and three-tick stall are not
  accepted as source truth merely because synthetic tests pass.
- Existing prior captures include Hyrule wall-collision experiments and the
  qualified Falcon Kick rebound route, but no normal-damage wall/ceiling
  tech-and-bounce oracle was found. The new route will reuse checkpointed
  headless Dolphin and manifest-driven numeric samples instead of creating a
  technique-specific test lane.

Execution results:

- [x] Add reusable, manifest-declared initial-state and collision-memory
  observations needed to place a physically launched fighter against a source
  wall or ceiling without modifying the response callbacks under test.
- [x] Capture wall tech, wall-tech jump, wall bounce, ceiling tech, and ceiling
  bounce across 719 source rows and 145 focused response observations. The
  accepted five-case live pack is 2.759 seconds warm and 5.493 seconds end to
  end.
- [x] Replace remaining placeholder timing/reflection behavior only from live
  observations plus pinned source/data, then register a generic stored domain.
- [x] Import `PlCo.dat` collision threshold `x1B0`, reflection multiplier
  `x1BC`, 15-frame reflection invulnerability `x1B8`, three-frame re-collision
  lock `x1C0`, five-frame wall freeze `x760`, and 14-frame wall-tech
  invulnerability `x764` without authored duplicates.
- [x] Preserve the source reflection action throughout hitstun; import
  31/45/26-tick wall-tech, wall-tech-jump, and ceiling-tech lifecycles from
  Falcon's submotion catalog plus the common five-frame freeze.
- [x] Match wall release 0.49/-0.13, wall-jump release 1.39/2.97, ceiling drift
  0.06 per tick, frame-11 1.99 control release, invulnerability boundaries,
  and preserved ceiling-tech hitstun within 0.0015 source units.
- [x] Register a five-case, 60-sample stored numeric domain. The root gate now
  runs four generated domains, 32 cases, and replay in 0.404 seconds on WSL and
  0.401 seconds on Windows.
- [x] Pass Windows 21/21, WSL 23/23, WSL ASan/UBSan 16/16, browser-adapter,
  collision-overlay, replay, and stored-oracle gates.
- [x] Update and validate the importer skill with the reusable runtime
  `MapCollData`, physical-waypoint, pre-contact trigger, response-field, and
  stage-geometry exclusion routine.

## Completed locally: flat-floor impact, missed tech, and tech response

Prior-art/source sweep completed before implementation:

- The pinned decomp route establishes the exact landing priority as directional
  tech roll (`ftCo_80098928`), neutral tech (`ftCo_8009872C`), then missed tech
  (`ftCo_80097D40`). No authored timing was accepted as source truth.
- `PlCo.dat` supplies the 20-frame tech window, 40-frame lockout, 0.2 roll-axis
  threshold, and 220-frame DownWait value. Falcon submotions supply 26-frame
  DownBound/neutral-tech and 40-frame directional-tech lifecycles plus the
  exact forward/backward `TransN` root tracks.
- Existing probes and tests did not form a pinned, same-input flat-floor domain;
  the new pack reuses the generic checkpoint, numeric trace, digest, and replay
  machinery.

Execution results:

- [x] Generalize the surface-response capture route once for both wall/ceiling
  and flat-floor manifests, including native collision during an X-only
  placement hold and a separately timed pre-contact trigger edge.
- [x] Import and validate the source tech window, lockout, direction threshold,
  and DownWait fields; content schema is now 74 and fighter schema is 67.
- [x] Preserve incoming self/knockback channels and stale hitstun memory on the
  landing callback frame, then project and decay ground knockback on the next
  tick in source order.
- [x] Replace the authored constant tech-roll speed with Falcon's imported
  forward/backward translation tracks through the existing allocation-free
  frame-data lookup.
- [x] Make snapshot validation distinguish active stun from Melee's retained
  hitstun memory so canonical hash/save validation accepts subsequent normal
  actions without weakening active-damage invariants.
- [x] Qualify four live cases over 804 rows / 232 focused observations. The
  stable semantic digest is
  `85fd93638bcb26b8b6e405cb1008a396acf05d132e07c7f9dcc3b6993034dd3f`;
  three independent captures agree after normalizing only landing-phase attack
  velocity and the two valid retained-hitstun magnitudes outside this domain's
  declared response semantics.
- [x] Compare 48 production samples on action/tick, invulnerability, preserved
  hitstun memory, and directional root translation within 0.0015 source units.
  Position remains assigned to stage/pushbox qualification. DownBound's source
  ECB contact toggles are now strictly qualified by the later pose slice.
- [x] Register the fifth stored domain. The root gate now covers 36 cases plus
  replay in 0.564 seconds on WSL and 0.543 seconds on Windows.
- [x] Pass WSL release 24/24 in 1.45 seconds, native Windows MinGW release 17/17
  in 1.05 seconds, focused WSL ASan/UBSan 4/4, and the browser adapter gate.
  The unavailable Visual Studio installation is not counted as a Windows
  result; the direct native Windows compiler lane is the recorded evidence.

## Completed locally: both prone/getup orientations

Prior-art/source sweep completed before implementation:

- The current `doldecomp/melee` head leaves the pinned DownBound, DownWait,
  DownStand, DownAttack, and input-helper callbacks unchanged. `libmelee`
  exposes the needed action labels but no replacement state semantics; the
  existing Slippi/ExiAI checkpoint transport therefore remains the reusable
  live-oracle path.
- Source option priority is buffered A/B or upward C-stick getup attack,
  horizontal C-stick edge or angle-qualified main-stick roll, then upward
  main-stick or fresh L/R neutral getup. Timeout selects neutral getup after
  the imported 220-frame DownWait timer.
- `PlCo.dat` owns the 0.2 main/C-stick magnitude threshold, 50-degree
  horizontal angle limit, 60-frame A/B buffer, and 0.6625 upward C-stick
  threshold. Falcon's generated catalog already contains the two 26-frame
  DownBound poses, two 70-frame DownWait poses, both 30-frame neutral getups,
  both 50-frame getup attacks, and all four 36-frame getup-roll `TransN`
  tracks.

Execution results:

- [x] Add one manifest-driven observation-segment primitive to the existing
  checkpoint route so delayed button/stick edges do not require another
  character-specific capture mode.
- [x] Capture and qualify the Down orientation across buffered A/B and upward-C
  getup attacks, main/C-stick rolls, neutral getup, option priority, timeout,
  exact action lengths, invulnerability, and root translation. Two independent
  2,370-row captures share semantic digest
  `fc91d42660ac0a8df8f0715b183b2ec97bccfe2ee0279491cadf915e64044438`.
- [x] Capture the opposite orientation and qualify posture-specific getup
  attack/neutral timing plus the source-selected roll motion and timing.
- [x] Implement and live-qualify the separately assigned DownBound ECB-driven
  contact transitions for both prone orientations.
- [x] Replace the authored getup thresholds, buffer, and constant roll speed
  only after live qualification; preserve the source input priority in one
  allocation-free common-state path.
- [x] Preserve raw A/B edges before projectile/item/special adapters consume
  effective inputs, pack C-stick edge state into the existing directional byte,
  and add only four bytes of canonical state for the combined A/B buffer age.
- [x] Register the resulting domain in the fast stored gate and rerun WSL,
  native Windows, sanitizer, browser, and replay validation.
- [x] Compare sparse production routes against independent live captures on
  action/frame, posture, invulnerability, option priority, roll direction, and
  root velocity within 0.0015 source units. Position remains an explicit
  separate pushbox/stage domain; DownBound contact is now strictly qualified.
- [x] Pass the latest WSL release 25/25 in 0.92 seconds, native Windows MinGW
  18/18 in 0.75 seconds, and focused WSL ASan/UBSan 5/5 in 6.80 seconds. The
  latest six-domain stored gate covers 46 cases and 120 prone samples in 0.465
  seconds on WSL and 0.628 seconds on Windows.

## Completed locally: quick/slow ledge-option lifecycle

- [x] Sweep pinned and current `doldecomp/melee` CliffWait, CliffClimb,
  CliffEscape, CliffAttack, and CliffJump callbacks plus maintained emulator
  harness prior art before extending the existing checkpoint architecture. No
  reusable exact ledge-option implementation replaces the pinned source path.
- [x] Capture quick and slow climb, roll, attack, and jump plus a drop probe in
  four concurrent headless/null/unlimited Dolphin shards. Moving the setup
  opponent away on the attack edge removes five unrelated hitlag holds from
  the action-owned trace. The clean pack contains 479 rows and measured 9.110
  seconds end to end under its 10-second guardrail.
- [x] Import exact CliffWait thresholds/timers, ledge roots, body-collision
  invulnerability, grounding frames, common jump impulse, and the Hyrule
  CliffJump wall-resolved path. Keep the latter as immutable qualified data so
  mirrored ledges share one allocation-free runtime path.
- [x] Preserve quick/slow source submotion identity through coalesced public
  ledge actions, including CliffJump1 to CliffJump2 and ordinary air physics.
  The compact neutral-ready latch reuses an existing serialized directional
  bit and does not increase snapshot or replay size.
- [x] Generalize numeric stored cases to optional per-case sample counts up to
  128 and teach the root verifier to sum those exact counts. Existing fixed-
  length domains retain byte-identical inputs and production digests.
- [x] Pin the clean live source digest
  `0882c32de5571a7fedec49d2b7e447bd46ccef930274b9603682239de57ce371`
  and production digest
  `3459f671fa8573a238b2d97bcec3a5fba1cb05a154b249cd02237cc2d1d88080`.
  All 8 cases / 450 samples pass on action, clock, facing, grounding,
  invulnerability, position, and velocity within the manifest-owned bounded
  Q16.16 envelope.
- [x] Register the focused CTest and ninth generic stored domain. The WSL
  focused test takes 0.02 seconds; all nine generated checks, 62 cases, and
  deterministic replay pass in 0.926 seconds. Full Windows, WSL, sanitizer,
  browser, and legacy-suite requalification remains the next gate.
- [x] Extend CliffWait qualification through the source IASA order: rising
  C-stick up attacks, rising inward C-stick rolls, jump, then the exclusive
  main-stick / drop-only C-stick direction router. Import `PlCo.dat` x7F8
  (0.6625) and x7FC (0.8) rather than reusing authored combat thresholds.
- [x] Add source-backed inward-roll, upward-attack, neutral-latched outward
  drop, main/C priority, held-before-ready, and sub-roll-threshold negative
  cases. The complete pack now contains 16 physical cases and 622 rows; its
  502 compared samples share source digest
  `6e404d19f093cb2bc082f8de21d6b10468550ce34d85949a084760423dbe2748`
  and production digest
  `64fcfecdb247d50a138021ceb316beb4380d176e8c99a186009f07d951e3d4a8`.
- [x] Make generated numeric domains own their exact total sample count, so
  variable-length additions cannot leave stale test reporting. The nine-domain
  gate now covers 70 cases in 0.943 seconds on WSL and 0.929 seconds on native
  Windows. Full release validation passes 28/28 on WSL in 0.49 seconds and
  26/26 on Windows in 3.15 seconds.
- [x] Bring the cold 16-case live pack back below its 10-second wall guardrail.
  EXI batching, a WSL-native pinned Python environment, and one preloaded
  parent forking six balanced capture workers reduce the complete pack from
  12.971 to 9.633 seconds. The generic runner validates an exact manifest case
  partition and keeps Dolphin executable names within Linux's 15-byte DME
  process-name limit. Eight Dolphins oversubscribe this 6-core host and are
  slower; three shards also lose to restore serialization.
- [x] Qualify both exact CliffWait timeout clocks from the internal
  `Fighter + 0x2344` motion variable rather than the looping displayed
  animation. The expanded 18-case pack contains 630 rows / 510 compared
  samples and pins source digest
  `9059582bb3abf01a99897935e9f27a6b78e2dec822936eec0adae62ed7b14f7e`
  plus production digest
  `0df9a4bbfc7f492e9c5e60f6e7d4a9af5358278768f55f959187d3b94ae48828`.
- [x] Preserve the source timeout transition into `DamageFall`/`TUMBLING`
  as the allocation-free public `AIRBORNE` state plus the existing tumble
  bit. The live comparison now checks that semantic bit explicitly while
  ordinary stick/C-stick drops remain non-tumbling.
- [x] Separate the live pack's warm execution budget from its honest cold
  startup budget. The final six-shard run passes at 10.666 seconds warm and
  11.036 seconds cold; no Python-import time is hidden. Full Windows Release
  passes 28/28 in 5.22 seconds and WSL Release passes 28/28 in 2.05 seconds.
- [x] Qualify the post-release ledge-regrab cooldown as the source-observable
  `29 -> 2 -> 1 -> 0` boundary. Production derives 29 from the imported
  30-frame common-data constant according to source callback ordering instead
  of carrying a second authored constant. The final 19-case pack contains 558
  rows / 514 compared samples and pins source digest
  `80c9ab75a98e2e7ef901055131b24f413c5e2672038315b526999f3143b38383`
  plus production digest
  `136f2c97c30bf041018014d0e1a5fb20caf0c942a77aca5a53ab465cd99adfcf`.
  A source-equivalent sparse timer jump, removal of unused observation tails,
  compact capture output, one inode-fingerprint hash per immutable oracle
  input, and one parent-side libmelee Dolphin-version probe shared by verified
  hardlinks reduce three independent complete runs to 9.649, 8.924, and 9.614
  seconds warm. Their cold lifecycle times are 9.969, 9.099, and 9.937 seconds,
  and all reproduce the same source digest. The manifest now enforces 10.0
  seconds warm and 10.75 seconds cold. The nine-domain gate now
  covers 73 cases in 0.908 seconds on WSL and 1.569 seconds on native Windows;
  full Release validation passes 28/28 in 1.96 and 4.37 seconds respectively.
  The final current-production verifier independently passes all 558 rows / 19
  cases / 514 samples against the latest simulator build with the same source
  digest.

## In progress: exact quick/slow ledge hurt poses

- [x] Reject ExiAI fast-forward pose capture after two clean runs preserved
  action, clock, and root state but produced different display-bone endpoints.
  Use checkpointed ExiAI input with fast-forward disabled and read all eleven
  collision-authoritative `FighterHurtCapsule` records from one contiguous
  fighter snapshot instead.
- [x] Reproduce the compact 450-row, eight-case capture byte-for-byte twice.
  The raw capture SHA-256 is
  `3055455eb02949e15c240f563a49648578b6c5affa4dc5dd7ca62f2c7b19c1e3`;
  its ten action tracks contain 434 exact displayed poses with semantic
  SHA-256
  `9125200e3e162822131fd8805ae1551371c4ebf0abc2256bba9a167cc181103a`.
- [x] Add one character-independent profile extractor and immutable C include
  generator. Falcon-specific code is limited to a data manifest and the
  zero-cost public-action/source-submotion binding; the shared pose lookup,
  world transform, combat collision, and inspection paths remain common.
- [x] Route quick/slow climb, roll, attack, and both jump phases through the
  generated 434-pose table. Strict Windows and WSL builds plus focused
  movement, combat, and deterministic replay tests pass.
- [x] Repin the intentional seeded match-soak identity after both Windows and
  WSL independently reproduced `52600d79f2b95349`; the prior digest did not
  include the newly routed ledge hurt geometry.
- [x] Extend the generic `falcon-common-hurt` stored-equivalence digest to all
  ten ledge tracks without copying their frame ranges. The generator consumes
  the hash-pinned profile, retains source-submotion identity, and hashes 689
  production poses under source/production digests
  `2aadf4b37b26796bdbc08fe026b234542f2c61914a4488e35e0dccd72a72e151` /
  `d691705692841bfabb8a2407ab31037bf398b097fc461574ecd07954e16a4331`.
  The complete eleven-domain plus replay gate passes in 1.12-1.14 seconds on
  Windows and 1.15-1.29 seconds in WSL.
- [x] Add the smallest source-qualified ledge hit/miss discriminator without
  inferring it from the exhaustive stored pose digest. Two independently
  launched two-shard captures are byte-identical at SHA-256
  `f31de47e694e46bf2269945747c97238ce443ddf88cbadc0a8e4214026f2785d`.
  Falcon Jab 1 frame 4 hits quick-climb frame 29 at the positive placement and
  misses 0.75 Melee units farther away. Reconstructed actual-pose margins are
  `+0.573037244` / `-0.098777672`; the generic rectangle remains a miss at
  `-1.895534515`. Semantic SHA-256 is
  `fbe0cf877402bf82aba10d8ae3dceecb4e431caa87d9b75b5844bfb7b132af2d`.
  Supplemental manifest projections and ordered semantic edges keep these two
  routes outside the ordinary 19-case ledge pack and the sub-two-second stored
  edit loop. The at-will live command passes in 6.07 seconds warm on this host.

## Completed locally: source-qualified Battlefield catalog and content

- [x] Sweep pinned `stage.c`, `ground.c`, `lbarchive.c`/JObj helpers, runtime
  `MapCollData`, and maintained stage-tooling prior art before extending the
  existing generic stage importer. `StageInfo` remains the authoritative
  runtime source for effective bounds and initial-player JObjs.
- [x] Extend the platform-only memory probe and schema-4 stage importer once,
  without a Battlefield-specific parser. The immutable catalog contains all
  23 dense collision lines (6 floors, 5 ceilings, 6 right walls, 6 left
  walls), runtime stage kind 36, four player points with floor supports, and
  effective camera/blast bounds after the source camera offset.
- [x] Pin the address-free semantic source digest
  `29525b7e0db4de8bf1a228f47e4216869ca362aff9d558a0c9ae81340103aa50`.
  Two independent 348-frame Dolphin captures regenerate the same compact JSON
  and generated C byte-for-byte; the second capture completed in 8.681
  seconds including process lifecycle.
- [x] Add one allocation-free reference-stage content constructor and O(1)
  line/spawn accessors. Battlefield content consumes imported blast bounds,
  uses imported floor/platform lines for its current stage primitives, and
  resets each fighter onto the floor directly below its corresponding source
  initial point. Pre-match entry/fall choreography remains outside this slice.
- [x] Make vertical arena validation symmetric so source-negative Battlefield
  top/camera coordinates remain valid through initialization and canonical
  state checks. One shared inline predicate covers fighters, items, and
  projectiles without runtime allocation or duplicated bounds rules.
- [x] Qualify catalog shape, exact transformed environment values, all four
  spawn/support mappings, content validation, and actual two-player reset.
  Focused combat passes in 0.40 seconds on WSL and 0.51 seconds with pinned
  native MSVC. Full-suite, browser, replay, and sanitizer qualification remains
  part of the final M4 gate.

## Remaining work, in priority order

### Falcon equivalence

- [x] Give open-air damage knockback its own source-equivalent velocity channel
  rather than folding it into ordinary self velocity.
- [x] Implement imported air magnitude decay in the exact source callback
  order and qualify the six-case open-air launch boundary live.
- [x] Qualify flat-ground knockback/friction decay and grounded damage-action
  release against a live source route.
- [x] Qualify wall and ceiling tech/bounce behavior against a live source route.
- [x] Qualify flat-floor landing plus missed, neutral, forward, and backward
  tech response against a live source route.
- [x] Qualify DownBound ECB pose-grounding in its own live source route.
- [x] Qualify imported Hyrule slope and ordinary ledge departure during floor
  recovery and promote the player-push route to the generic stored architecture.
- [x] Preserve all seven displayed `CliffCatch` frames, import absolute
  `CliffCatch`/`CliffWait` root anchors, and qualify the first wait frame rather
  than collapsing ledge acquisition directly into hang.
- [x] Preserve imported JumpF/JumpB and JumpAerialF/JumpAerialB clocks through
  their exact Fall/FallAerial successors; qualify the ordinary eight-frame
  Fall loop and the complete captured aerial-jump route.
- [x] Qualify quick/slow ledge climb, roll, attack, and two-phase jump through
  exact action clocks, grounding, invulnerability, Hyrule collision-adjusted
  roots, and common airborne physics.
- [x] Qualify neutral-latched main/C drop arbitration, main-versus-C-stick
  priority, threshold-adjacent negative inputs, and C-stick attack/roll edges.
- [x] Qualify the full 640/480-frame CliffWait timeout boundary, including
  its source `TUMBLING` exit.
- [x] Qualify the drop/timeout regrab-cooldown boundary.
- [x] Import and independently reproduce the complete Battlefield collision
  catalog, effective camera/blast bounds, and four initial-player points.
- [x] Import and independently reproduce CrouchWait's complete looping ECB,
  including canonical source-frame advancement and rollback retention.
- [ ] Replace remaining authored damage, hitstun, launch, collision, and input
  behavior with imported data or explicitly documented gaps.
- [ ] Convert each completed fidelity family into a generic manifest-driven
  live and stored domain.
- [ ] Exercise positive, threshold-adjacent negative, and entry/exit boundary
  routes for every primitive; do not add dedicated tests for emergent
  techniques.
- [ ] Close every incomplete or partial row in
  `docs/product/m4_ssbm_fidelity_audit.md`.

### Test infrastructure

- [x] Generalize checkpoint-pack route selection beyond the original
  common-hurt command-line mode.
- [x] Extend the generic stored C runner from geometry domains to numeric
  trace/transition domains without character-specific runner loops.
- [x] Keep changed-domain local validation comfortably below two seconds. The
  current nine-domain, 73-case stored gate is 0.904 seconds on WSL and 0.925
  seconds on native Windows. Live manifests carry explicit
  per-pack budgets: paired player push
  is 0.090 seconds warm; wall/ceiling is
  2.759 seconds warm; the larger 804-row floor pack measured 2.752 seconds on
  its final run with a four-second guardrail; the 1,515-row, 14-case prone pack
  is physically sharded and measured 9.812 seconds end to end with an
  18-second guardrail; the original 479-row ledge-option pack measured 9.110
  seconds. Its expanded timeout/regrab-qualified 558-row / 19-case pack passes
  three independent runs at 9.649, 8.924, and 9.614 seconds warm after sparse
  timeout acceleration, compact output, hardlink-aware source hashing, and one
  parent-side libmelee version probe for the immutable Dolphin inode.
- [x] Make the expanded ledge pack repeatably complete within ten seconds on
  this six-core host without loosening provenance or semantic coverage. The
  manifest enforces a 10.0-second warm guardrail and all three accepted runs
  reproduce source digest
  `80c9ab75a98e2e7ef901055131b24f413c5e2672038315b526999f3143b38383`.
  Six shards remain faster than seven or eight on this host.
- [x] Meet the affected-domain three-second warm target without source-invalid
  cross-invocation reconnection. The common-hurt route measures 2.635-2.729
  seconds warm, while each packed invocation keeps one Slippi observer attached
  across its checkpoint-isolated cases. A prototype that attached a new observer
  to an already-running process resumed a desynchronized event stream and is
  deliberately rejected; it is not part of the accepted architecture.
- [x] Maintain this roadmap and explicit coverage ledgers; no finite scenario
  may be described
  as detecting every possible anomaly.

### What a green equivalence result means

Each registered domain is a small executable theorem, not a universal anomaly
detector. Live headless Dolphin establishes pinned NTSC 1.02 observations;
the simulator adapter receives the corresponding inputs and compares only the
manifest-declared fields and tolerances; the approved production digest then
makes the ordinary no-Dolphin edit loop bit-exact. Replay hashes additionally
prove deterministic continuation, but do not establish Melee fidelity on their
own. Whole-Falcon equivalence is reached only by closing every fidelity-audit
row with positive, negative, threshold, entry, and exit coverage.

The reusable proof path is:

1. A pinned manifest declares the owner disc/revisions, route, cases, observed
   fields, tolerances, source files, expected digests, and time budget.
2. One live Dolphin process restores a known checkpoint for each independent
   route or branch, applies the declared inputs, and records source state.
3. A semantic source verifier also checks the relevant decomp code/data, so a
   coincidentally matching trace cannot silently replace the intended rule.
4. The simulator runner applies the corresponding inputs through shared C
   adapters and compares only the domain's declared state and geometry.
5. Once reviewed, generated stored cases and production digests make the fast
   Windows/WSL edit loop independent of Dolphin while preserving provenance.

This architecture catches regressions only inside registered domains. The
ground-knockback live route, for example, deliberately excludes position even
though flat-floor player push is independently registered; a green result must
not be reported as proof of unregistered slopes, ECBs, ledges, attacks, or
other stage/pushbox topologies.

### Native playtest frontend

- [x] Provide a validated source-derived Battlefield content constructor for
  the client, including current floor/platform primitives, blast bounds, and
  player reset supports.
- [x] Replace the M1 SDL render-packet spike with a real native simulation
  client.
- [x] Render the complete source Battlefield line catalog, fighters, stocks,
  damage, actions, hit/shield geometry, blast bounds, crouch, and essential
  state cues.
- [x] Expose and render the complete source hurt-capsule set in the native
  collision inspector instead of treating the fighter body cue as exact hurt
  geometry. The append-only inspection schema is now version 54; both native
  spawn poses expose all 11 ordered capsules and are covered by the focused
  combat gate.
- [x] Support keyboard and SDL GameCube-controller input with independent
  analog triggers and both sticks.
- [x] Provide reset, pause, frame-step, and collision-inspection controls.
- [x] Add native match setup without duplicating simulation configuration
  policy in the presentation layer.
- [x] Verify the native client on Windows and WSL/Linux: both full Release
  suites pass 28/28 in 3.35 and 1.92 seconds respectively after the current
  source-`Pass` qualification slice.
- [ ] Complete hands-on GameCube input qualification and preserve the existing
  cloud CI coverage when the branch is published; continue other work while
  CI runs.

### Importer skill

- [x] Add the reusable HSD archive and `ftLoadCommonData` routines discovered
  in the current slice to the personal `ssbm-character-importer` skill.
- [x] Document anisotropic world-coordinate conversion before DI/knockback
  vector operations.
- [x] Document separate self/knockback velocity channels, source callback
  ordering, and imported air-magnitude decay for reuse by later characters.
- [x] Document absolute animation-root placement for `CliffCatch`/`CliffWait`,
  body-center conversion, catch lifecycle qualification, and snapshot/ledge-
  ownership validation.
- [x] Document canonical source submotion retention when a target engine
  coalesces multiple Melee motions into one public action, including imported
  transition/loop lengths and directional predicates.
- [x] Document clean base-Wait entry, blend-completion qualification, DAT-owned
  wait weights/blend bytes, exact global HSD RNG/rejection ordering, seed
  isolation, transactional draws, and terminal-plus-one blend sources.
- [x] Document that a complete rendered line catalog is not a complete stage
  collision port: floor, ceiling, right-wall, and left-wall ranges need
  independent runtime routing, action-specific ECB timing, selected-line and
  normal evidence, and response qualification.
- [x] Document that `Pass`'s imported 30-frame submotion clock and common-data
  platform collision-skip timer are independent, plus the relocation method
  for capturing an animation ECB that natural landing would truncate.
- [x] Document exact sloped wall/ceiling qualification: complete top/side ECB
  schedules, earliest point/line intersection, pre-transform source normals,
  and the gravity-before-total-velocity reflection order. The personal skill
  validates after the update.
- [x] Validate the current skill update and exercise its `PlCo.dat` reader
  against the owner extract (145824-byte data block, 23 pointers).
- [ ] Forward-test a future character-style task when an update is substantial
  enough to merit it.

### Validated: Battlefield wall and ceiling routing

- [x] Sweep pinned and current `doldecomp/melee` `mpcoll.c`/fighter collision
  paths plus current libmelee before changing production. The relevant source
  files are unchanged; libmelee's projection helper explicitly omits ECB
  evolution and is unsuitable as an exact runtime oracle.
- [x] Replace the reference-stage central-rectangle wall and ceiling checks
  with allocation-free Q16.16 queries over the imported immutable line ranges.
  The same catalog now drives ordinary movement, hitlag/SDI rejection, and
  stationary wall-contact probing without duplicating stage geometry.
- [x] Exhaustively exercise the five ceiling and twelve wall catalog entries,
  motion-direction and geometry negative controls, plus representative
  production contact against Battlefield's central underside and side wall.
- [x] Bound the compact approximate-airborne platform compatibility path to
  the immediately preceding crossing, and bypass it for a source-qualified
  exact ECB schedule. A fighter placed fully below a pass-through line remains
  airborne instead of teleporting upward; imported JumpF lands on source frame
  95 without the compatibility path's extra tick.
- [x] Review the intentional seeded-soak ripple and repin only after three
  Windows and three WSL runs reproduced `70dda9b2e5d9d936` with identical
  8-match / 2,847-tick event counts, rollback, and replay results.
- [x] Complete Windows Release 28/28 in 1.74 seconds, WSL Release 28/28 in
  1.95 seconds, WSL ASan/UBSan 21/21 in 13.50 seconds, and both nine-domain
  stored-oracle lanes (73 cases plus replay) in 1.01-1.13 seconds. The rebuilt
  native smoke passes and the interactive client has been relaunched.
- [x] Add identical-input live Battlefield qualification for JumpF/Pass ECB
  contact timing, selected floor-line identity, and absolute vertical
  resolution through the production reference-stage content.
- [x] Add selected-normal and normal-based response qualification plus live
  Battlefield wall/ceiling routes. Two checkpoint-isolated cases select source
  line 10 / ceiling normal `(0.378117, -0.925758)` and line 15 / right-wall
  normal `(0.569210, -0.822192)`, then compare 24 response samples across
  action time, displayed frame, state, timers, self/knockback velocity, and
  relative position. Source and production digests are
  `8a0c463ffae10b1567815013c85c500bcb25869727874086c96d0e9c522a2f68`
  and `107ea657a7bad069ea8ee02cb98306dd116b78838c8e6899a4adf9ff6fcf0982`.
- [x] Replace DamageFly rectangle contact with its complete 24-frame top and
  side ECB schedule and an allocation-free earliest segment intersection.
  Reflect against the imported source-space unit normal after applying the
  frame's gravity to total motion; do not integrate remaining reflected motion
  on the contact tick. This closes the wrong-ceiling selection and the 5.6%
  horizontal wall-response error found by the numeric verifier.
- [x] Reproduce the source digest in a second fresh Dolphin process. The warm
  two-case pack takes 0.367 seconds and the complete launch/menu/capture cycle
  3.096 seconds.
- [x] Validate Windows Release 29/29 in 1.70 seconds, WSL Release 29/29 in
  1.89 seconds, WSL ASan/UBSan 22/22 in 13.86 seconds, and all ten stored
  domains / 75 cases plus replay in 1.03-1.05 seconds. The 348-frame production
  Battlefield route still passes on both hosts within 640 Q16 units. Three
  Windows and three WSL soaks reproduce digest `d3b4c23cb8a9dd7e` with the
  same 8-match / 2,847-tick outcomes before repinning.

### Validated: reflected-action ECB evolution and floor re-contact

- [x] Extend the checkpoint capture protocol with one-shot post-entry fighter
  velocity reset and synchronized fighter/collision position reset. This keeps
  the native wall/ceiling collision and action transition intact while moving
  only the later observation away from the blast zone and stage surfaces.
- [x] Capture the complete observable Falcon response clocks in one 0.739-second
  warm two-case pack: `BOUNCE_CEILING` frames 0..8 (frame 8 then remains held)
  and `BOUNCE_WALL` frames 0..50, with top, bottom, left, and right ECB points
  on every row.
- [x] Canonicalize the source poses facing-right and generate immutable full
  top/bottom/left/right Q16.16 tracks: nine ceiling poses and 51 wall poses.
  Raw/profile/semantic identities are
  `f1989a139185635d41d5cc2a51b0f88d41c1a26cf24c57fa82614feed6fda1c2`,
  `d6ccb5701f0bada0d7de1874004281e8ca46fcc0070db94e529d84d3fc637608`,
  and `9d162fe7917f0c23894ad1fe54a1a665d5c8e446d5ca439180811d706b2431a5`.
- [x] Route both actions through one allocation-free full-pose adapter shared
  with DamageFly. The real action clock continues while ceiling presentation
  clamps to pose eight; facing is applied only at the world-space wall side.
- [x] Live-qualify 111 focused updates from a native bounce entry through the
  first Battlefield top-platform re-contact. Ceiling lands on sample 57 and
  wall on sample 54 with source action, real clock, grounded state, hitstun,
  invulnerability, facing, relative position, self velocity, and knockback
  velocity all agreeing inside the existing 16/640-Q16 bounds. Source and
  production trace identities are
  `4e9a0ad3222bd0d6b6d7ab7def0177cf4b5c361bded3826abfe2e91f9210dd5a`
  and `222a5504d62bc5500e57a88a0adad108b931ea73d2b70cdf46faccde3f36d2db`.
- [x] Register the reusable numeric domain as the eleventh fast gate. Three
  warm Windows all-domain runs take 1.123-1.136 seconds; WSL takes 1.121
  seconds. Windows/WSL Release, deterministic replay, byte-identical Falcon
  regeneration, Python compilation, and WSL ASan/UBSan all pass. The focused
  CTest takes 0.03-0.04 seconds, and fresh Windows/WSL native-client smoke
  binaries pass without replacing the already-running Windows playtest.

### Validated: landing entry and source `Pass`

- [x] Follow `ftCo_Landing_Enter` through `ftCommon_8007D7FC` and preserve the
  incoming vertical self velocity on the `Landing` entry row; project it to
  zero on the next grounded-physics update, matching the saved source trace.
- [x] Follow `ftCo_8009A228`, `ftCo_Pass_Anim`, `ftCo_Pass_Phys`, and
  `ftCo_Pass_Coll`; retain raw submotion 209 inside the existing allocation-free
  public-`AIRBORNE` source-submotion adapter instead of adding a duplicate
  public action or schema field.
- [x] Capture all source `Pass` frames 0..29 by moving the fighter only after
  action entry. Pin raw SHA-256
  `0dc57f8ffb85549be76b3b5a0017690b0df16905456169eaceaa2e7975eedc0c`
  and semantic frame/ECB SHA-256
  `90060e614f359189c32b25d76b780b3fa92861dfdcfae0fd357dcc07ec10e6f8`;
  generate the complete immutable Q16.16 bottom-ECB table byte-identically.
- [x] Pass the full 348-frame identical-input Battlefield movement capture on
  Windows and WSL within the existing 640-Q16 position envelope. The former
  frame-276 divergence now lands on the same source frame with the same
  incoming vertical velocity.
- [x] Capture all 35 displayed JumpF ECB frames with the same post-entry
  relocation method. Pin raw SHA-256
  `28c4e902d8860f6d02ec779004c67c7ab94f87c7f3970699cfd9a44a8844cf1d`
  and semantic frame/ECB SHA-256
  `6db927d319942e07d90ba6dd30aad39ad40bb42ab3cc09d498ea2587bfe233bb`;
  use the exact table to remove the approximate crossing delay on source
  frame 32 and enter `Landing` on observed route frame 95.
- [x] Run the identical inputs through `pf_m4_reference_stage_content` instead
  of an authored vertical mimic and compare the selected imported support on
  every grounded source row. The left-platform phase selects source line 2
  (support 3), and the final Pass landing selects main-floor line 1 (support
  2), identically on Windows and WSL.
- [x] Repin downstream deterministic identities only after three Windows and
  three WSL runs reproduced verifier digest `3c3f20d38cee6e59` and identical
  8-match / 2,847-tick event counts. Replay final/event digests remain
  unchanged; its content-bearing corpus digest is now
  `02f52e1f9c9dbf29e21264c50d2139b8968c6bff810da3b30e00d9ba34fb2e0b`.
- [x] Complete Windows Release 28/28 in 1.91 seconds, WSL Release 28/28 in
  1.78 seconds, WSL ASan/UBSan 21/21 in 13.66 seconds, and both nine-domain
  stored lanes (73 cases plus replay) in 1.008-1.168 seconds under manifest
  SHA-256 `f16fa189ecb621a54c6ed4921aa920a257316b0fbeb869fb21caa08104ccefb3`.
- [x] Close live Battlefield selected-normal, normal-response, wall, and
  ceiling qualification through the dedicated two-case theorem above.
- [ ] Continue the remaining edge-acquisition/action-specific ECB audit. The
  special-callback inventory confirms Falcon Dive is the only Falcon special
  that calls the combined ground-and-ledge query, and its bidirectional route
  is already qualified. Ordinary down-input rejection, all five aerial-attack
  floor-contact tracks, both complete 26-frame DownBound ECB tracks, both
  70-frame DownWait loops, and all eight getup-option tracks are closed.
  All 26 ordinary ground/aerial action routes are now source-qualified and
  production-routed; the remaining work is broader special/common acquisition
  and stage-shape coverage. Passing selected floor/wall/ceiling routes does
  not claim whole-stage or whole-Falcon equivalence.

### Validated: reusable native-trace Raptor Boost oracle

- [x] Reuse `pf_m4_movement_trace` directly through the generic
  `native-csv-trace-v1` stored schema instead of cloning a Falcon-specific C
  adapter. Manifest input phases expand offline and compress to deterministic
  runs; the verifier parses only declared integer fields.
- [x] Preserve the live comparator boundary exactly with per-case field masks
  and half-open per-field sample exclusions. This retains every qualified
  action-tick row while excluding only hitlag-frozen and special-landing rows
  where the live comparator deliberately has no clock assertion.
- [x] Reuse one shared controller normalization and Raptor action/timer mapping
  in the live comparator and independent source projection. The source gate
  verifies that all 502 stored inputs are identical to the pinned captures.
- [x] Pin five fighter routes under source/production SHA-256
  `19b5d604d5721e20bc2151e41c11054632a5c384dfd5528cf373dac2bd1abe2c` /
  `7733655e234ac2de12fe1b674ed6be967ad7de39b848d94cf97b2e36547509a0`.
  The 155-frame native Capsule search remains a separate live-only branch
  because the project's Relay Rod intentionally is not a Melee Capsule.
- [x] Run independent native cases concurrently while retaining manifest order
  in the canonical digest. Six Windows all-domain runs take 1.435-1.550
  seconds and three WSL runs take 1.448-1.585 seconds for 14 domains / 89 cases
  plus deterministic replay, below the two-second edit-loop budget.
- [x] Register the focused Raptor stored gate in ordinary CTest/CI. Windows and
  WSL Release both pass 33/33 tests; the added cross-platform test takes 0.36
  seconds on each host.

### Validated: natural airborne callback order and complete landing ECB

- [x] Follow the pinned decomp's interrupt-to-physics update order. A double
  jump entered from an interrupt callback runs the new JumpAerial physics
  callback in the same fighter update, so its imported 0.9 horizontal impulse
  is immediately followed by ordinary air control. Production now performs
  both operations in that order without a character-specific state branch.
- [x] Import complete JumpF, JumpB, JumpAerialF, JumpAerialB, Fall, and
  FallAerial bottom-ECB tracks: 186 immutable Q16.16 poses from two
  byte-identical 494-row captures. Profile/semantic SHA-256 are
  `407a62269b2aa65002bb4a78152f12a49b56d36d8b68a684c6d55a11ce69a1ba` /
  `21a2d02fbb3abfcd9c29bb170c4c378fc8972fe191098fb5587140e965dac25a`.
- [x] Replay two independent 520-frame natural Battlefield captures through
  the same Windows and WSL production runner. Actions, clocks, facing,
  grounded/support state, surface normals, and velocities are strict; position
  uses only the existing 640-Q16 accumulated-conversion envelope. Source and
  production trace SHA-256 are
  `43eb893b7d70852b03b696c993db09e08ee4554bee5922c2a214aef73da7bf95` /
  `9e8fd5b0c3ee8d065c0fef6c906c9700d1600d6ef34e7eb498735a25ed81f26b`.
- [x] Register the route through the generic native-CSV stored kind. The
  fifteen-domain / 90-case gate plus deterministic replay passes three Windows
  runs in 1.592-1.678 seconds and three WSL runs in 1.279-1.578 seconds, below
  the two-second edit-loop budget. Byte-identical regeneration, Python syntax,
  focused movement/combat, and replay pass on both hosts.
- [x] Reproduce the first CI result locally: the headless lane's only failure
  was an incidental player-1 attack-recovery transition landing on the final
  player-0 KO tick after the corrected trajectory shifted that tick. The match
  test now settles the setup attacker before asserting the final-stock journal;
  all 27 headless tests and strict Windows MSVC pass.

### Validated: Falcon Kick stored oracle and edge-conversion ordering

- [x] Recheck current upstream decomp against pinned revision `9509dc0`; the
  Falcon Kick state table, attributes, and callback implementation are
  unchanged at current revision `6ddd74e`.
- [x] Reuse the generic `native-csv-trace-v1` runner and factor the live
  Falcon Kick action/timer mapping into shared pure projections. Six
  hash-pinned ground, air, landing, edge, hit, and Hyrule wall routes bind 399
  source samples under source/production SHA-256
  `2c6f28a9701990b913adb2f2daa214433bb18174a610af6c96fc1dce39deaf33` /
  `19a4dd302f0e51fa9d01d8fe7193d57e1b3ea5979e496fb6138bc0c85f356f4e`.
- [x] Let the live rerun reject a one-frame regression before accepting the
  stored digest. Ground-origin edge conversion now enters its aerial end state
  with zero self velocity and suppresses ordinary air physics on that update;
  all six live routes pass again on WSL.
- [x] Repin the intentional seeded verifier soak only after Windows and WSL
  independently reproduced 2,848 ticks and digest `b9239b63a68a1a18` for all
  eight twin/save-load/replay matches. The deterministic replay corpus remains
  unchanged.
- [x] Run independent registered domains concurrently, retaining deterministic
  manifest order for counts and the final digest. The complete 16-domain /
  96-case gate plus replay now takes 0.430-0.508 seconds on Windows and
  0.384-0.408 seconds in WSL, down from 1.758-1.819 seconds immediately before
  the domain-level concurrency refactor.

### Validated: all five aerial-attack ECBs, shared airborne lock, and natural landing callbacks

- [x] Sweep current `doldecomp/melee` prior art before implementation. Pinned
  and current `ftCo_AttackAir_Coll` both route through floor collision without
  the ordinary Fall callback's `ftCliffCommon` branch, so aerial ledge catch is
  intentionally excluded rather than implemented from a false hypothesis.
- [x] Capture complete Nair/Fair/Bair/Uair/Dair ECB tracks twice without
  display-bone-skipping fast-forward. Both independent runs reproduce 195
  poses and semantic SHA-256
  `55e686a07cf3d064618104051f0085ed2a398e9a1612847200b2cba51a665f10`.
  The raw HSD evaluator matches top/right/left in all five routes within one
  Q16 unit; bottom is deliberately qualified through the separate common-lock
  lifecycle because it retains the previous desired bottom during conversion.
- [x] Preserve distinct previous and current action/submotion clocks when
  sweeping animated ECB endpoints. Reconstructing the previous endpoint from
  the post-callback pose can miss a crossing and drop the fighter through the
  platform.
- [x] Replay two independent 685-frame natural Battlefield captures. Five
  concurrent cases prove Nair/Fair/Dair landing-lag entry and Bair/Uair
  auto-cancel under source/production SHA-256
  `83e1fadc017af2c5005411ea2fae8d378855127662196fa0b9b81a37c7a11efe` /
  `dd14d194d15a115925c17761fd5fff692413b26a268ef96286180e176273e490`.
  Only each landing-entry row excludes libmelee's stale incoming self-Y field;
  action, elapsed clock, support, normal, and subsequent velocity remain exact.
- [x] Generalize the existing natural-movement verifier into one shared
  multi-case source projector. The 17-domain / 101-case gate plus replay takes
  0.575 seconds in WSL and 1.458 seconds on Windows. Movement, combat, replay,
  deterministic regeneration, and ASan/UBSan pass.
- [x] Repin the deterministic replay only after three identical reruns. The
  common-lock correction yields corpus/final/event SHA-256
  `7de13f6a61f41619113c004203979d889a3603d2d8e5a60cd0ed2fab96d7a35f` /
  `7d031c271e05fb0041fa749488689175fb6b775f44d58a794bc1aa1e1c47bd48` /
  `55581ad6489814368e540e8eb96779ece01d840b1dd6ce7899afd1c4f724ac6bd`.
- [x] Replace the packed 195-value aerial bottom table with the shared decomp
  rule from `ftCommon_8007D5D4`: enter air with lock 10, decrement before map
  collision, retain the previous desired bottom while locked, then expose the
  live HSD bottom. A production double-jump integration gate verifies stored
  lock 9 after entry, eight displayed Nair frames at the inherited bottom, and
  the frame-9 unlock. The parent-closed profile now has 50 motions, 3,366
  tracks, and 37,366 keys under decoded-data SHA-256
  `caab1daafb4b54c836b1eee697ebe01935780561ed5ddaf421c3039ea4d7a552`.
- [x] Pass all 41 CTests on rebuilt Windows and WSL trees. The full 21-domain /
  117-case stored gate plus replay passes three isolated Windows runs in
  0.984-1.062 seconds and three WSL runs in 1.295-1.754 seconds without
  changing the two-second budget.

### Validated: Rapid Jab Start and source-owned ECB reference space

- [x] Sweep pinned decomp revision `9509dc0`, current upstream revision
  `d882af9`, and Falcon's complete DAT submotion metadata before changing the
  evaluator. `ftCo_800D6B00` enters Attack100Start with zero blend; the former
  entry-blend diagnosis was false.
- [x] Reproduce `ftAnim_8006E9B4` / `ftAnim_8006E054` ownership. Submotion
  animation flag `0x80000000` extracts and zeroes TransN before collision, so
  those motions subtract the animated reference joint. Motions without that
  flag retain TransN in the skeleton while `mpColl_LoadECB_JObj` subtracts only
  fighter `cur_pos`, so their ECB is evaluated in model-root space.
- [x] Generate the decision from each imported submotion's existing
  `animation_flags`; no Rapid-Jab branch, duplicate table, parser, allocation,
  host floating point, or rollback field is added. The same reusable rule is
  shared by production and all source qualifiers.
- [x] Close Rapid Jab Start across both existing independent Dolphin captures.
  The complete ordinary-action theorem now covers 26 motions, 2,086
  observations, and 1,850 unique frames with maximum one-Q16 error. The native
  primitive covers all 925 represented action poses.
- [x] Expand the parent-closed profile to 51 motions, 3,424 tracks, and 37,533
  keys under decoded-data SHA-256
  `2e1bec542d6c3ae6ce21f814039bab2b81caf05f2eac03b05ecd0d0118189bd2`.
  The shield-break branch verifier now validates its complete four-track
  semantic profile before selecting the requested live branch track, closing
  a stale multi-track/profile provenance inconsistency.
- [x] Rebuild and pass all 41 Release CTests on both native Windows and WSL.
  The complete 21-domain / 117-case stored gate plus replay passes in 1,120.723
  ms on Windows and 1,486.147 ms in WSL, under the existing two-second budget.

### Validated: ordinary damage animation identity and imported HSD geometry

- [x] Sweep pinned decomp revision `9509dc0`, current upstream revision
  `d882af9`, and Falcon's complete DAT motions before implementation.
  `ftCo_8008DCE0` selects the ordinary damage motion from pre-launch
  ground/air state, knockback level, and collided hurtbox height. Ground-to-air
  conversion does not reselect the motion. `Fighter_ChangeMotionState` plus
  `ftAnim_8006EBA4` exposes source frame one immediately with no entry blend.
- [x] Reuse the existing full collision-memory probe; no browser probe or new
  Dolphin protocol was added. Two independent 138-row physical captures have
  raw SHA-256
  `e34454e4f4cd7c3e02d46285820ce8210b9c002f6a32242577fba98aa9f0e437`
  and
  `24dc8291bcfe9ca8e470bda95e34e97242eb1138a5fc356eef91746777201401`.
  Their 72 DamageN2 rows share the exact source-frame pattern: frame one is
  held through hitlag, then frames 2-10 advance one per resumed update.
- [x] Extend the reusable source qualifier with manifest-owned repeated frame
  patterns, label exclusions, capture-owned grounded/ECB-lock state, and
  component diagnostics. Across both captures, 276 pose observations / 3,036
  hurt capsules agree within one Q16 unit. The 132 rows after the hit-entry map
  callback also reproduce all four ECB points within one Q16 unit. The six
  mixed-ownership entry rows remain explicitly excluded: the new action and
  skeleton are already visible while collision still owns the preceding ECB.
- [x] Import raw submotions 165-179 into the existing parent-closed profile.
  It now contains 66 motions, 4,381 tracks, and 44,149 keys under decoded-data
  SHA-256
  `d013285272bfe3c4ad7a52218d24dbc7aabda24293289fbc06445fd51ae68109`.
  Production threads the first accepted hurtbox's height through the existing
  collision winner reduction, uses one constant source table, and reuses the
  canonical animation cursor and allocation-free HSD evaluator without new
  rollback fields, runtime parsing, floating point, or duplicate pose data.
- [x] Extend the existing flat-ground knockback oracle rather than creating a
  new framework. It asserts DamageLw1 selection, frame-one hitlag freeze,
  resumed source-clock progression, every live production hurt capsule, and
  mid-damage canonical save/load. All 24 ground/air, damage-level, and hurtbox-
  height table entries are also checked against the decomp table.
- [x] Requalify deterministic dependents after the intentional source-identity
  change. Replay corpus/final/event SHA-256 values are now
  `7f210b0b70d2a506f60da411d4212885a5714ddc816c6fb076ad6273939a5ef0` /
  `7d031c271e05fb0041fa749488689175fb6b775f44d58a794bc1aa1e1c47bd48` /
  `55581ad6489814368e540e8eb96779ece01d840b1dd6ce7899afd1c4f724ac6bd`.
  The Hyrule slope/ledge production digest is
  `bf8b2f390b2246835678a49ce191120ac4b8f39a4fb82130e9df5675354ac8a4`;
  all source samples remain within their existing strict/Q16 envelopes.
- [x] Pass Windows MinGW Release 39/39 and WSL Release 41/41. The complete
  21-domain / 117-case stored gate plus replay passes in 898.577 ms on Windows
  and 825.175 ms in WSL, below the two-second edit-loop budget; focused WSL
  ASan/UBSan combat also passes. The character-importer skill now owns the
  reusable damage-motion and mixed callback-ownership procedure.

### Validated: pinned UCF 0.84 PAD/cardinal preprocessing

- [x] Capture 16 three-row main- and C-stick boundary cases twice under the
  pinned GALE01 NTSC-U revision-2 plus UCF 0.84 target. The independent 48-row
  capture SHA-256 values are
  `d6d7cb26d0b30785bb38c39a6b400366742998d6f9f2eeb448f4a7cb31db4984`
  and
  `b46ef4c579a26050f6cb8f9eda6c6c5068dd5b62ac65eae4ccdc0d9847075372`;
  their canonical projections are identical.
- [x] Use Slippi's serialized raw main- and C-stick bytes as the current-input
  authority and the source fighter's DME-observed processed axes as the
  post-UCF authority. The captured raw-80/orthogonal-6 snap cases and raw-79
  or orthogonal-7 controls reproduce the pinned modifier on both sticks.
  Browser and ordinary processed-input fallback paths are not claimed as
  exact raw-controller evidence.
- [x] Canonicalize the live and native traces through the shared
  `native-csv-trace-v1` path. All 16 cases / 48 samples are structurally equal
  at source/production SHA-256
  `4a553ba57522d4347188cb227357157fbb4f1a7246dd638fba68019e9166fd63`.
  Direct verifier-only execution takes 232.529-251.415 ms on Windows and
  453.341-678.188 ms in WSL.
- [x] Keep the physical pack bounded: its two four-worker runs take 7.349 and
  6.883 seconds. Parent warm time is 8.097/7.357 seconds and cold time is
  9.728/8.722 seconds; the manifest enforces 12-second warm and 20-second cold
  budgets.
- [ ] Live-qualify the six remaining UCF hook boundary families: DBOOC, SDI,
  shield SDI, tumble, shield-drop suppression, and the extended pad counter.

### Completed: large pinned Slippi diagnostic sweep

- [x] Download and hash-pin a substantially larger public Falcon corpus before
  comparing it. The MIT-licensed `erickfm/melee-ranked-replays` archive at
  dataset revision `11142d4b86d423716fdd2e9ca565de9bafc9d37e` contains 782
  Falcon `.slp` files; the first 300 were copied into the ignored local corpus
  and recorded in a manifest with per-file SHA-256 values.
- [x] Replace the one-at-a-time parser bottleneck with a bounded eight-worker
  Slippi extraction pool while retaining manifest order for all report rows and
  summary counts. The 300-entry run took 224.764 seconds and completed 300
  replay parses, 259 natural anchors, 53 native comparisons, and 1,303
  semantic frames.
- [x] Preserve the reference boundary in the large run. The public `.slp`
  files do not independently prove the GALE01 revision-2 disc or exact UCF
  0.84 build, so every comparison is explicitly diagnostic-unverified. The
  run produced 21 diagnostic passes, 3 UCF dashback boundary observations, and
  32 deterministic diagnostic candidates; none is promoted to an exact
  simulation gap without a pinned source capture/code configuration.
- [x] Keep the downloaded archive and reports out of the tracked registry. The
  hash-pinned 300-entry manifest is
  `9e13187aa364de9315414fc4c56a7e99e3521b3ed100c618d7f32a0d96600f9a`; the
  ignored report is `build/slippi-differential/ranked-300-report.json`, SHA-256
  `3f1b62cd3be996b51234669b8684669b3146a58bd7c5fb5912e891bf68cd99bc`.

## Completion gate

M4 is not complete until all three top-level deliverables are present and
verified: optimized reusable equivalence infrastructure, source-complete and
route-qualified Falcon behavior, and a playable native Battlefield frontend.
Passing one stored domain or one replay corpus is evidence only for its stated
coverage.
