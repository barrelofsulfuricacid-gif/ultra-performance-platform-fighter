# M4 combat vertical-slice progress

**Status:** In progress; M4.1 movement/ledge core, the complete grounded
jab/tilt/dash-attack/directional-strong vocabulary, charged directional smashes,
M4.2 hit-reaction layers, missed-tech floor recovery, dense and analog light
shield with health/strength-derived collision volume, tilt, pokes, grounded
horizontal shield SDI/ASDI, and physical powershield cancel, solid stage
geometry, and wall/ceiling tech
plus directional air dodge, helpless fall, wavedash/waveland,
ledge-cancelling, 29-tick ledge-regrab lockout and planking, the complete
ledge-roll and ledge-attack option set, the complete five-direction
light-aerial vocabulary and direct strong aerial route,
auto-cancel, visibly scored
L-cancel practice, SHFFL, grounded forward/backward rolls, and spot dodge
plus explicit first-airborne-frame instant double jump
plus a local stock-select setup, delayed respawn, invulnerability, sudden death,
results, rematch/return-to-setup, the bounded rollback-safe typed event feed, and complete
  shield-break launch/down/stand/stun/recovery, the three-tick small-step
  forward-smash route, the hitlag-assisted same-platform drop cancel,
  reduced-down shield platform dropping, three-frame V-cancelling, grounded
  low-percent crouch cancel, target-weighted shared hit reaction, and
  ordinary-input approach, spacing, mindgame, cross-up, juggling, ladder,
  kill-confirm, zero-to-death, platform-sharking, jump-canceled-grab, and
  pummel, directional-throw/chain-grab, and jab-reset routes, plus two-pad
  browser polling and a verifier-readable collision inspector implemented

**Accepted baseline:** `5cfb263d9ba322da0bf330b75e3c7e656a15043a`

**Working branch:** `agent/m4-combat-vertical-slice`

## Implemented and verified in the 2026-08-11 mixed damage-entry follow-up

- Closed the six formerly excluded hit-transition ECB rows without changing
  production state. Melee and production both run movement/map collision
  before attack collision and damage entry, so the transition observation has
  DamageN2 frame-one hurt capsules but the preceding Wait variant's
  post-animation ECB.
- Generalized the source verifier with declarative
  `previous-row-post-animation` ownership instead of a Falcon-specific table.
  Two independent physical captures now qualify 288 observations / 3,168 hurt
  capsules, including 12 mixed ECB rows, at maximum one-Q16 error.
- Added the split-consumer callback-order procedure to the reusable
  `ssbm-character-importer` skill. Broader damage interactions and stage routes
  are the next open fidelity surface.

## Implemented and verified in the 2026-08-11 DamageFlyTop/Roll follow-up

- Imported the exact common-data top cone, 100% Roll threshold, and HSD random
  boundary; the shared transaction-local RNG is now threaded through combat
  without adding state or a second random stream.
- Imported Falcon submotions 180/181 into the generic HSD profile. Roll's
  source-coordinate velocity angle overwrites `XRotN` in one reusable local
  pose evaluator used by hurt collision, inspection, floor, wall, ceiling,
  and ECB queries.
- Reused existing accelerated Dolphin surface/prone evidence. A fresh
  response-only capture passes all 145 rows across five cases at semantic SHA
  `5339134dd04cff9612e8c8a3e1d460f85018ae4c081ac7426fbad3cee3b785f5`.
  The verifier no longer hashes derived world-space line fields that duplicate
  the pinned raw stage catalog. Its single-process wrapper still measures
  4.234 seconds warm against the three-second performance gate; forking
  Dolphins was measured and rejected because repeated menu startup is slower.
- Windows and WSL Release pass 41/41. The complete 21-domain / 117-case stored
  gate passes in 1.081 seconds on Windows and 1.011 seconds in WSL; common-data
  and dynamic-HSD regeneration checks are clean.

## Implemented and verified in the 2026-08-11 CrouchWait ECB follow-up

- Generalized the checkpoint shard runner so a manifest selects either the
  damage-hit or common-hurt-geometry capture route. The merger now handles
  both `_setup -> _observe` and `_place -> prefix` case families without a
  Falcon-specific merge loop.
- Extended the surface-memory probe with Falcon's live motion, animation,
  fractional frame/rate, and blend fields. The reusable cyclic ECB extractor
  canonicalizes captures that begin anywhere in a loop, rejects non-adjacent
  modulo order, and proves repeated frames are deterministic.
- The authoritative five-case / 303-row capture has SHA-256
  `cb07f5c3bff1f55e7f223e3863822a6d023bb6adf9ad13b69918111fcb341ba6`.
  A separate one-worker process captured 160 CrouchWait rows in 5.307388
  seconds warm (9.062093 cold), with raw SHA-256
  `6d66a2e7e88f6264fb4932c7395d0a1344f8548da5ec2b68100c244c7e749c82`.
  Both independently reproduce all 158 frames under semantic SHA-256
  `ba47ef2736a5677d1909262a20f32991b7c2515407fae26626d5869b95edd265`.
- Production stores the exact four-point cycle in the generated Falcon table
  and selects it in constant time from the existing source submotion and Q16
  animation cursor. CrouchWait now owns a one-frame source animation clock;
  tick canonicalization, save/load validation, inspection, floor, wall, and
  ceiling queries all retain the same clock without a new rollback field.
- The first Windows full gate found that the old canonicalizer deliberately
  zeroed every non-Walk/Run cursor. Separating general source-clock ownership
  from velocity-driven update semantics fixed that architectural gap. The
  resulting verifier digest `a58e07aa84531219` repeated identically three
  times before repinning. The final Windows and WSL Release gates pass 39/39.
  The unchanged 21-domain / 117-case stored gate plus replay passes in 0.764
  seconds on Windows and 1.433 seconds in WSL.
- The first sanitizer run exposed a pre-existing signed overflow in the shared
  Hyrule wall-contact fraction `(numerator * 65536) / denominator`. The common
  collision ratio helper now uses the direct division for safe operands and an
  exact fixed 16-step unsigned division only for the overflow range. Native
  output remains bit-identical and the sanitizer-only arithmetic fault is gone.
  Replacing the current ECB with the actual previous-tick pose then exposed an
  uninitialized transition path when the previous action was `HITLAG`. A shared
  inline effective-action helper now resolves the resume action and the wall
  sweep has a deterministic fallback. Ten consecutive optimized WSL traces
  reproduce digest `73f3dae4bf726aedd1e2ab37911818faa9b3fff4d1a19ed2a92a41148f142f5d`;
  WSL ASan/UBSan passes 25/25 in 24.26 seconds.
- Fractional Walk/Run ECBs remain explicitly open: source-evaluator probes
  reached 0.142 Melee-unit ordinary error and a 1.52-unit WalkMiddle outlier
  when a tiny pose delta crossed `mpColl_LoadECB_JObj`'s 10-unit symmetry
  branch. Those motions remain on the existing collision path until the HSD
  blend and branch behavior are exact.

## Implemented and verified in the 2026-08-11 DownWait/getup ECB follow-up

- Added two projection-only Dolphin cases for rolls selected after entering
  `DownWaitU`; the normal 14-case prone-response pack remains unchanged. This
  closes the source distinction where terminal `DownBoundU` deliberately uses
  the D roll motion while `DownWaitU` selects the U roll motion.
- A four-worker headless/null/unlimited route captures 1,150 response rows in
  10.5-10.9 seconds and reproduces both 70-frame DownWait loops, both 30-frame
  neutral getups, both 49-frame getup attacks, and all four 35-frame roll
  motions: 438 complete top/bottom/left/right ECB poses.
- The canonical profile has file SHA-256
  `9c3dfc58d1f34acf1ff264fc443d70e0ba283f5bd09da71bb7134fe8e8e9a1e0`
  and semantic SHA-256
  `f519d632a88bcb582cb68865dd9a58d27e862fe619fc05d76ff3252ad5204f19`.
  An independent live process regenerated the semantic digest exactly.
- Production stores the poses in orientation/direction-indexed tables and
  resolves them in constant time through one shared prone ECB adapter. The
  70-frame DownWait source phase is preserved as `0..69`; runtime entry maps
  action tick zero to displayed frame 1 and wraps through frame 0.
- The Windows focused combat/profile gates pass, and repeated verifier runs
  agree on content-bearing digest `f6d9ef1b4f85e04f` with unchanged
  8-match / 3,002-tick event counts. Windows Release passes 36/36 in 1.49
  seconds, WSL Release passes 38/38 in 0.92 seconds, and WSL ASan/UBSan passes
  25/25 in 9.22 seconds. The 21-domain / 117-case stored gate plus replay passes
  in 0.932 seconds on Windows and 1.723 seconds in WSL, below its two-second
  budget.

## Implemented and verified in the 2026-08-11 DownBound ECB follow-up

- Audited every Falcon special collision callback. Falcon Punch, Raptor Boost,
  and Falcon Kick intentionally perform ground/wall collision only; Falcon
  Dive is the sole Falcon special that calls Melee's combined ground-and-ledge
  query, matching the already-qualified production predicate.
- Reused the existing prone-response checkpoint pack with an explicit
  unbatched geometry mode. A two-case headless/null/unlimited Dolphin capture
  records 600 rows in about 6.6 seconds and reproduces all 26 displayed
  `DownBoundD` and 26 displayed `DownBoundU` four-point ECB poses.
- Added the canonical 52-pose profile under semantic SHA-256
  `3c4a4ce4586b11617aa99a08bac8709ea6d7aa8a179b5494c6f3f7fe4785c7df`.
  Production resolves it through the shared constant-time ECB path for
  action-specific wall and ceiling queries while retaining the independently
  qualified floor-contact schedule. Source DownBoundD maps to the canonical
  stomach variant and DownBoundU maps to back; the binding is manifest-derived.
- Added a fast committed-profile CTest and an at-will live verifier. The live
  repeat regenerated the same semantic digest from a new Dolphin process.
- Windows Release and WSL Release pass 37/37; WSL ASan/UBSan passes 25/25 in
  20.86 seconds. The full 21-domain / 117-case stored gate plus replay passes
  in 0.712 seconds on Windows and 0.658 seconds
  in WSL. Repeated verifier runs agree on the new content-bearing digest
  `c0248444d9d95ff6` with unchanged 8-match / 3,002-tick event counts.

## Implemented and verified in the 2026-08-10 decomp differential

- Audited pinned NTSC 1.02 common callbacks and all Falcon special callbacks
  directly against production. Closed every discrepancy visible within the
  represented state/data model; no runtime experiment or guessed frame datum
  was used.
- Added exact source-routed mash/capture escape, shield-break and Furafura,
  Damage/DamageFall IASA and buffering, crouch/smash knockback modifiers,
  electric hitlag, meteor cancel, repeated-hit velocity merging, rebound/clank,
  rebirth timers, teeter, grab/release, throw weight scaling, and pummel hitlag.
- Added Falcon Jab 3 and rapid-jab lifecycle, down-tilt repeat, angled forward
  tilt/smash variants, exact smash charging, C-stick defensive buffering, and
  source input-priority arbitration. Authored moonwalk states are bypassed for
  source Falcon movement.
- Corrected raw submotion identity throughout: `Pass` is 209,
  `Ottotto`/`OttottoWait` are 210/211, `CatchCut`/`CaptureCut` are 246/257, and
  shield-break fly/down/stand motions are 286-291. Earlier notes that called
  `Pass` submotion 244 were wrong; 244 is an action-state identity, not
  Falcon's raw DAT submotion slot.
- Advanced compatibility to content 76, fighter 68, state 69, save format 64,
  `PFSAVE58`, a 775-byte payload, and a 915-byte checkpoint. Only callback-
  consumed history/continuation is canonicalized.
- Validation then exposed and closed two source-projection regressions: the
  Dash frame-4/5 input boundary now enters DashAttack on the pinned source
  route while ground-damage animation continues after hitstun unlocks; Falcon's
  ordinary JumpAerial is no longer mislabeled as a six-frame delayed-cancel
  state. Pinned live verification passes 64 ground-knockback rows / 15 compared
  samples and both 520-row natural-landing captures.
- Production browser startup no longer executes duplicated simulation suites.
  The C adapter shrank from roughly 14,000 to 1,100 lines and its install ABI
  from 62 integers to four real configuration values. Browser-specific gates
  retain adapter wiring, controller polling/mapping, UI, Wasm loading, and real
  interaction coverage.
- Windows Release passes 32/32 in 3.45 seconds; WSL Release passes 34/34 in
  3.81 seconds. All 18 stored domains / 108 cases plus replay pass in 0.843
  seconds on Windows and 0.783 seconds in WSL. WSL ASan/UBSan passes 25/25.
  The rebuilt Emscripten playtest, browser verifier, in-app-browser controls,
  and console smoke pass.
- Exact geometry for pummel/capture, remaining common poses, and shield-break
  orientation paths remains an import/evidence boundary. Dynamic
  rebirth targets, companions, broader stages/items, aerial item/tether
  callbacks, and match choreography require a wider model.

## Implemented and verified in the executable ordinary-geometry follow-up

- Consolidated the former independent ordinary hit- and hurt-geometry inputs
  into one 2,974-row Dolphin 3.5.1 memory capture. It now covers Jab 3, rapid
  jab, every forward-tilt angle, and Falcon's real high/mid/low forward-smash
  variants in addition to the previously imported actions.
- All routes use natural controller/action transitions. Angled tilts age their
  diagonal input during Landing's locked frames before the first IASA update;
  no action or animation frame is mutated. Jab 3 imports only its executable
  frames 1-12, rapid jab canonicalizes each repeated active window to its
  source command phase, and the non-executable forward-smash-low frame-63
  extractor spill is rejected by live memory.
- Two independent headless/null/unlimited captures have raw SHA-256
  `aeff75c16b2041fbecc6b8ec2322a614e0695f0d3d9088eb44d60aedbdeb7ca0`
  and `5a797d05fe1dfd30ee1a82b7ede3cac3c003a668d20dcd1d53b824450e19bd55`.
  Their address-free, route-qualified semantic projections match across all
  2,974 rows. Display-bone fast-forward is intentionally disabled because it
  breaks state-3 previous/current moving-sphere identity.
- Generated production geometry identity is
  `652d912618489111cd78541321f32c0f56e3d495380d0b7e1182a1bce4e4a1f7`.
  Strict Windows and WSL Release pass 34/34 in 0.83/0.77 seconds, WSL
  ASan/UBSan passes 25/25, and all 18 stored domains / 108 cases plus replay
  pass in 0.645 seconds on Windows and 0.539 seconds in WSL.

## Implemented in the TurnRun display-facing closure

- Current and pinned `doldecomp/melee` agree on `ftCo_TurnRun_Anim`: frozen
  frame 9 resumes and flips gameplay facing before display bones evaluate the
  next animation step. The existing capture contains six old-facing frame-9
  rows and exactly one row with new gameplay facing plus old display facing.
- Production now derives collision-pose facing from the unique existing state
  tuple `TurnRun`, action tick 10, and `facing == dash_direction`. Combat and
  collision inspection use that derived facing; rollback/save schemas and
  replay hashes remain unchanged.
- The generic stored oracle adds a reusable pose-facing case mode. Falcon's
  domain has adjacent controls for pre-flip frame 9, pending-display frame 9,
  and resumed frame 10, raising the complete registry to 18 domains / 108
  cases. The live verifier binds those cases to the existing independent raw
  captures and unchanged semantic SHA-256
  `1cc3543b1363ecb5c7427c36f4d8d8a2826f9fb7c5281877f54108e1ffe281a2`.
- Full stored equivalence plus replay takes 0.614 seconds on Windows and 0.605
  seconds in WSL. Windows/WSL movement, combat, and replay pass; WSL ASan/UBSan
  passes. The character-importer skill now records the reusable phase-case
  pattern and validates successfully.

## Implemented in the Turn/TurnRun hurt-pose follow-up

- A focused two-case checkpoint pack now captures only `Turn` and `TurnRun`:
  39 rows in 0.227533 seconds warm with ExiAI fast-forward and 0.565447
  seconds with fast-forward disabled. Independent raw captures have SHA-256
  `e0240567b226cdd1802a3b7ad14384cf5096df9c8b323b383020c0a1844cf901`
  and `18abfc0b39cc15614dcda03e243abb8298adcd4738869ea1738cf550cbed6be5`.
- Both captures reproduce the same 33-pose / 363-capsule semantic payload,
  SHA-256 `1cc3543b1363ecb5c7427c36f4d8d8a2826f9fb7c5281877f54108e1ffe281a2`.
  Turn covers frames 1-11; TurnRun covers source frames 0-21 and explicitly
  classifies the seven-observation frame-9 freeze plus its one pending-display-
  facing row.
- Production retains source submotions 10/11 for the two public actions and
  uses one immutable generated table. The separate stored domain hashes 33
  poses under production digest
  `0750e32d78d2b51b49f4e917dde6088a266e6940d92351250c8a167422563d07`.
  The full registry was 18 domains / 105 cases plus replay at this checkpoint;
  the later display-facing closure raises it to 108 cases.
- Source-submotion retention intentionally changes the replay corpus identity
  to `0d3ccb293d0735102c13d020d469f13b202eede2b54052881d0380efb765e172`;
  final-state and event digests remain unchanged. Windows, WSL, replay, and
  WSL ASan/UBSan suites pass.
- The one-update display-facing phase identified here is closed by the later
  TurnRun display-facing follow-up above.

## Implemented in the native Wii U adapter follow-up

- Windows identified the owner's switched adapter as `WUP-028` USB
  `057e:0337`. That mode intentionally does not expose the four ports through
  the Gamepad API, which explained the complete controller loss after moving
  the hardware switch away from PC mode.
- The browser playtest now offers `Connect Wii U Adapter`, claims the native
  interface with WebUSB after an explicit owner gesture, sends `0x13`, and
  continuously decodes the adapter's 37-byte reports. Raw main stick, C-stick,
  analog triggers, buttons, and all four occupied-port indicators are retained;
  the first two occupied ports map to the two local players.
- The former PC-mode `0079:1843` Gamepad path remains supported. Wii U mode
  avoids the DirectInput normalization boundary that made a full physical
  cardinal flick difficult to distinguish from the fastest walk on this
  hardware.
- A deterministic native-report mapping probe covers axes, D-pad override,
  A/X/Start/Z/L, C-stick, and analog R. The Chrome smoke requires the WebUSB
  control and mapping-probe result, while granting real USB access remains an
  owner interaction.

## Implemented in the gradual-stick fast-walk follow-up

- The midpoint-based follow-up also overcorrected: unlike Melee, it committed a
  low first sample to walking even when the stick reached full horizontal on
  the next frame. The production rule now follows the pinned decomp's global
  X-tilt timing: any first sample above the movement threshold starts tick 1,
  reaching the dash threshold on tick 2 still dashes, and reaching it on tick
  3 or later remains the fastest walk. No new canonical field or schema change
  is required.
- The Mayflash `0079:1843` mapping now applies the existing 0.75 gate
  normalization to the main stick as well as the C-stick. A physical cardinal
  flick therefore reaches full simulation magnitude instead of landing at or
  just below the dash threshold because of adapter rounding.
- The superseded controller-accessibility retune authored
  `dash_input_window_ticks=1`: any sampled intermediate horizontal magnitude
  committed to the fastest `WALK` route, while only a direct neutral-to-full
  sample dashed. It proved too strict for ordinary analog dashes and was
  replaced by the balanced low-entry rule above. Final-state and event-stream
  replay identities remained unchanged through both revisions.
- Research against Melee's `ftCo_Dash_CheckInput` and horizontal stick-tilt
  timer confirmed that dash is a magnitude-plus-time decision: the stick must
  reach the dash region while the two-frame timer remains open. There is no
  midpoint-based early walk commit.
- The initial fidelity implementation authored `dash_input_window_ticks=2`.
  `WALK` action ticks retain the bounded tilt age, so a gradual neutral-to-full
  ramp stays in `WALK` at `walk_speed_q16`; the accessibility retune above later
  narrowed this authored window to one sample. Neutral resets the window.
- Content schema 54/fighter schema 47 hash and validate the immutable timing.
  Native and Wasm input probes cover both a three-sample aged fast-walk route
  and a low-then-full two-sample dash without adding canonical mutable fields.
- The schema-driven replay corpus identity is repinned to
  `43f635c1b1b8ef72d24e8dd0b1163c9f0bc3a70f08d78b52b40997238cc6adc1`;
  its final-state and event-stream SHA-256 values remain unchanged. The
  eight-match verifier digest and 2,488/1,088-byte state/scratch requirements
  also remain unchanged.

## Implemented in the sampled half-moon Moonwalk follow-up

- The remaining failure was the center of the physical half-moon, not its
  lower-back endpoint. Melee's dash action remains active while the stick
  passes through down/neutral, and its horizontal tilt timer ages out the
  dashback check; the browser build instead routed the same sampled input to
  crouch/platform-drop or idle before it ever reached back.
- Production `INITIAL_DASH` now preserves a lower-half sweep through saturated
  forward-down, down, and lower-back samples. The existing `MOONWALK_SETUP`
  state records that traversal, reconstructing the continuous neutral crossing
  that can occur between two 60 Hz browser samples. Straight back then enters
  the facing-preserving Moonwalk. Small vertical noise and a direct immediate
  straight-back input retain ordinary dashback behavior.
- Native and Wasm probes now execute the complete half-moon instead of starting
  at its lower-back endpoint. The direct two-tick lower-back/notch and reduced
  horizontal alternatives remain supported.

## Implemented in the Falcon-easy Moonwalk input slice

- Research and the Melee dash implementation agree that Moonwalk uses ordinary
  initial-dash physics: a backward input below the dashback threshold applies
  backward acceleration without changing facing. Falcon players normally use
  the controller's lower-back diagonal notch for at least two frames before
  moving to straight back.
- Production input now recognizes that lower-back diagonal during
  `INITIAL_DASH` and `MOONWALK_SETUP`, before crouch/platform-drop or dashback.
  This remains reliable when a browser/DirectInput adapter independently
  saturates both diagonal axes. The authored two-tick minimum, facing retention,
  backward velocity, traction slide, and mistimed straight-back dashbacks are
  unchanged.
- The native oracle and Wasm startup probe now perform the natural fully
  saturated lower-back-notch route. Browser help documents the GameCube notch,
  keyboard Down-plus-opposite equivalent, and the retained Shift fallback.

## Implemented in the jump-takeoff momentum slice

- The first grounded jump now applies a fighter-authored takeoff formula after
  ordinary jump-squat traction: scaled ground velocity plus the current
  horizontal-stick contribution, clamped to the authored jump maximum.
- The default original fighter uses a 0.8 ground-momentum multiplier, 0.18
  full-stick contribution, and 0.28 takeoff cap. Running forward, pressing
  jump, then immediately holding backward through the remaining three-tick
  jump squat produces an almost in-place launch while keeping the original
  facing. Forward and neutral controls retain long and intermediate travel.
- Content schema 53/fighter schema 46 hash and validate all three values. The
  native movement oracle and existing Wasm air-facing startup probe cover the
  reverse, neutral, and forward takeoff routes through the production input
  and simulation paths; no technique-only canonical state was added.
- The copied fighter data increases the opaque simulation-state requirement
  from 2472 to 2488 bytes. Scratch remains 1088 bytes, and both stay within the
  public 4 KiB storage bounds; serialized state/save and browser layouts do not
  change.

## Delivered in the first M4 slice

- A validated, hash-identified `pf_m4_content` precursor containing one
  original placeholder fighter table and one original test-stage table.
- Real-simulation Q16.16 states for proportional walk, initial dash, run,
  dash-dance reversal, run turnaround, run brake, post-turnaround run lockout,
  facing, traction, crouch, jump squat, binary short/full hop, configured air
  jump, independently steerable aerial drift with takeoff-facing lock, fast
  fall, landing, moving-platform support, normal and shield platform drop,
  support edges,
  solid-block top/side/underside collision with sealed upper-corner seams,
  blast-zone stock loss/respawn, ledge catch, catch lockout, hang, release,
  ledge jump, climb, roll, and attack.
- Deterministic one-fighter-per-ledge occupancy with stable lower-slot priority
  for simultaneous catches.
- A rollback-safe state-schema-25/save-format-24 contract that serializes every
  future-affecting movement, attack, hit-reaction, ground-tech, and current
  shield and match field plus the authoritative event sequence.
- Replay format 1 regenerated against the new canonical state schema and real
  attack/hit inputs, with native and WebAssembly comparisons still using the
  same corpus path.
- A public `pf_m4_inspect` surface for movement state, active ledge claims,
  ledge points, moving-platform and solid-block geometry, blast zones, percent, hitlag,
  hitstun, tumble, tech timers, SDI state, active hitbox bounds, and last-hit
  metadata, plus shield health/stun/powershield state, trigger age, and
  L-cancel eligibility, stocks, respawn timers, sudden death, and result.
- Two hundred forty-three movement/content invariants plus a 20,000-tick
  four-player
  canonical-state trace under the active `M4-MECHANICS` verifier entry.
- A live two-player browser adapter that advances the production simulation at
  fixed 60 Hz, draws its inspected stage/player state, and supports pause,
  single-step, reset/rematch, stock HUD, respawn countdown, and result overlays.
- Explicit full-magnitude dash/dash-dance keys and reduced-magnitude walk keys
  for both keyboard players, with the real binary jump-squat selection rule.
- A native and Wasm startup contract that refuses readiness unless walk,
  dash-dance reversal, short/full-hop apex, aerial landing/L-cancel timing,
  instant-double-jump timing, held-input rejection, and double-jump-cancel
  timing, momentum cancellation, late-input rejection, and knockback-based
  counter armor with late/strong-hit rejection,
  strong-aerial 30/15-tick landing timing, real damage/hitlag,
  shield-break phase/mash/recovery, and reaction-input and stock/respawn
  invariants pass.

## Delivered in the collision-inspector slice

- The live browser now exposes a default-on, pause-safe collision inspector
  through both the toolbar and the `I` key. Toggling it redraws the last
  inspected frame without advancing the deterministic simulation.
- The overlay distinguishes the exact inspected floor, moving one-way
  platform, solid block, and blast-zone boundaries; player hurtboxes, active
  attack hitboxes, and active grabboxes; and Relay Rod plus Pulse Bolt
  collision extents. Invulnerable hurtboxes retain their geometry but use the
  existing dashed-gold rejection semantic.
- The visible legend and stable DOM attributes make the same semantics readable
  at the generated-page boundary. `browser-runtime` proves generated-page
  initialization and the published inspector surface. The separate
  `browser-collision-interaction` acceptance remains an explicit owner gate for
  toggling and visually checking the overlay; a DOM dump does not claim that
  interaction. The former source-grep overlay check was retired as duplicate
  churn. Tolerant screenshot comparison remains the separate planned M7
  visual-reference check.
- This is a presentation/verifier slice over already inspected canonical data.
  At that slice, browser view schema 33 remained 396 values and no simulation, replay, save,
  observation, or RL format changes.

## Delivered in the temporary M4.3 local-match flow

- The browser now opens in an explicit local 1v1 setup state instead of
  advancing the match behind the player. It identifies the fixed placeholder
  fighter, test stage, 60 Hz simulation, and keyboard/standard-gamepad routes.
- Stock choices 1–4 rebuild the production duel through
  `pf_sim_config.stock_count`; invalid values fail closed and the selected
  value is immediately visible in inspection and both player HUD records.
- `Start Local Match` clears queued setup input and begins at tick zero. Match
  controls are disabled during setup, then enabled for play; the existing
  terminal result pauses the match and offers both Rematch and Change Setup.
  Time-limit truncation now receives the same Rematch label as a stock result.
- The generated-page `browser-runtime` acceptance checks initialization and the
  live setup surface. `browser-match-flow-interaction` remains an explicit owner
  gate for starting a match, reaching results, rematching, and returning to
  setup; it is deferred on the non-interactive DOM-dump lane. This temporary
  M4.3 surface does not claim the broader M7 menu-navigation system.

## Delivered in the replay-file event-visualization slice

- Replay observer schema 1 exposes checkpoint zero and every subsequently
  hash-verified tick from the existing format-1 container, including the exact
  ABI-4 event journal re-emitted by simulation. The native replay corpus proves
  the observed event-stream digest equals the source `PFEVT001` digest.
- The WebAssembly inspector can download its verified canonical replay and open
  a compatible `.pfreplay` file. It publishes a replacement trace only after
  chunk checksums, identity, every state hash, and the terminal result pass.
- The replay timeline now renders typed events at their checkpoints and offers
  Previous event / Next event navigation alongside positions and SHA-256 state
  hashes. The file-import surface is bounded to 1 MiB and the current canonical
  four-player, 500-tick content/config fixture.
- `browser-runtime` proves the generated replay surface initializes, and the
  clean browser smoke requires the file control, stable visualization
  semantics, and re-simulated canonical events. The separate
  `browser-replay-interaction` acceptance remains an explicit owner gate for
  Previous/Next-event navigation and fail-closed incompatible-file import; it
  is deferred on the non-interactive DOM-dump lane. The former source-grep
  replay check was retired.

## Delivered in the M4 benchmark-workload slice

- Canonical `representative_1v1` scenario version 2 now initializes the actual
  M4 content extension with combat, item, projectile, reflector, charge, and
  recovery fixtures enabled. Its 240-tick ordinary-input cycle covers fighter
  hitboxes, a live projectile, shielding, and at least one combat event before
  timing is accepted.
- `maximum_combat_entities` is no longer a future-capability placeholder. Its
  bounded four-player team cycle uses only ordinary pickup, throw, projectile,
  attack, and shield inputs and resets through `pf_sim_reset`; it performs no
  state injection. The untimed preflight requires all four hurtboxes, at least
  two simultaneous fighter hitboxes, at least four simultaneous attacking
  entities, overlapping item and projectile hitboxes, and a typed event.
- The representative workload's scenario-version bump deliberately starts a
  new compatibility series instead of comparing M4 combat measurements with
  the former pre-M4 movement trace. Ten of the thirteen canonical scenarios
  are now measurable; hazards, workbook import, and client-frame timing retain
  their later-milestone availability reasons.
- Two clean 15-repetition, 100-ms-target milestone runs on both WSL GCC 13.3
  and native Windows MSVC 19.44 qualified all ten available scenarios with no
  invalid comparison, suspected regression, or confirmed regression. The
  production 1v1 workload varied by +1.92% on WSL and -2.03% on Windows; the
  maximum-combat-entities workload varied by +0.70% and +1.11% respectively,
  all within their same-target non-regression envelopes.
- A clean Tracy 0.13.1 profile-only capture at exact grounded-shield-SDI commit
  `f55f07e` records all thirteen
  workload slots, ten measured scenarios, frame marks, and canonical zones.
  WSL uses the recorded timer fallback; the raw trace remains local and the
  platform-profiler claim remains unavailable because `perf` was not installed.
  Two fresh native-Windows milestone runs at the same exact commit qualified
  all ten available scenarios against an isolated same-commit baseline with
  zero invalid comparisons, suspected regressions, or confirmed regressions.
  See the [M4 performance checkpoint](../../performance/reports/2026-08-01_m4_combat.md)
  and [profile analysis](../../performance/profiles/M4/analysis.md).

## Delivered in the repeated verifier-match slice

- The authored-C verifier now runs eight seeded, one-stock production M4 duels
  through the public input and inspection APIs. The bots use ordinary movement,
  attack, strong-attack, jump, shield-trigger, and special inputs; a late legal
  movement policy prevents an inert match from being mistaken for coverage.
- All eight reference duels finish through stock results rather than time-limit
  truncation. The current reference corpus spans 1,203 ticks, 19 combat
  events, eight KOs, and five projectile events, with final digest
  `a7c9d1cc1f812ba0` over every per-tick state hash and terminal outcome under
  state schema 41/content schema 43.
- Every duel advances a lockstep twin, saves a 24-tick checkpoint, reloads and
  re-simulates the complete suffix against per-tick state hashes, encodes a
  format-1 replay, and verifies that replay to the exact terminal outcome.
  `M4-VERIFIER-MATCHES` is an active acceptance row backed by this internal
  production-path check.
- This match soak covers the playable loop and constituent mechanics. It does
  not add tactic-specific harnesses for emergent techniques.

## Delivered in the owner-evidence capture slice

- The generated WebAssembly page now includes a collapsible 61-row owner
  checklist sourced at configure time from the versioned M4 registry. Each row
  carries the registry's exact human recipe, an observed pass/fail/untested
  state, and optional reproduction notes; configuration fails if any ordered
  row cannot be parsed.
- Evidence-schema 1 records tester, full build reference, browser/OS/input
  environment, the eight M0 combat-rubric scores, critical collision status,
  completed setup-to-result/rematch and repeated-match gates, real Standard
  Gamepad use, overall notes, and an explicit approve/request-changes decision.
- Drafts persist locally under the pinned source revision. Markdown and JSON
  exports include all 61 outcomes plus runtime-probe status, while a two-step
  reset avoids accidental deletion. The page deliberately cannot promote the
  technique registry; owner evidence must be exported, reviewed, and committed.
- Tactical and emergent rows reuse their independently verified constituent
  mechanics. This presentation-only slice adds no emergent-specific simulation
  harnesses and makes no new deterministic-state, replay, save, observation,
  RL, or performance claim.

## Delivered in the first M4.2 combat slice

- Input-schema-3 light- and strong-attack buttons for native, replay, RL, and
  browser callers.
- Two original data-driven grounded attacks with independent startup, active,
  recovery, hitlag, damage, launch, and facing-mirrored hitbox geometry.
- Deterministic hurtbox overlap, one-hit-per-action masks, lower-slot
  same-target ownership, team friendly-fire rejection, and simultaneous
  trades.
- Q16.16 percent, percent-scaled launch, hitlag freeze, pending launch,
  hitstun control lockout, gravity/landing continuation, and blast reset.
- A monotonic rollback-safe combat-event sequence with per-target last-hit
  tick, attacker, and damage, now paired with typed per-tick event records.
- Twenty-eight focused attack/reaction invariants, mid-hitlag save/load
  continuation, and a 20,000-tick four-player deterministic combat trace under
  active `M4-COMBAT` verification.
- Browser light/forward-smash controls (`F` and `/` or Numpad `0`), direct
  strong-attack controls (`H` and `'` or Numpad `2`), action-colored
  active-hitbox overlays,
  percent/hitlag/hitstun inspection, and independent combat/tumble probes.
- The default strong attack enters tumble on its first clean hit. Browser
  fighters visibly rotate during post-hitlag tumble so the state is apparent
  without relying on the diagnostic card.

## Delivered in the first hit-reaction slice

- Fixed-point trajectory DI read on the final hitlag tick, with a data-defined
  18-degree maximum and deterministic launch-speed renormalization.
- Component-edge SDI during target hitlag plus final-tick ASDI, with
  data-defined thresholds and distances and collision-safe positional shifts.
- Data-defined tumble threshold and explicit `KNOCKDOWN`, `TECH_IN_PLACE`, and
  `TECH_ROLL` action states.
- A rising analog-trigger edge opens the 20-tick tech window and 40-tick
  lockout; held input cannot retrigger it.
- Neutral and directional ground-tech outcomes on floor and pass-through
  platform contact, with explicit locked-action durations.
- Melee-style 26-tick tech-in-place and 40-tick tech-roll durations with exact
  20-tick hit invulnerability, vulnerable missed tech, browser inspection, and
  positive/negative hit-rejection coverage.
- Browser vertical DI and tech inputs, live tumble/SDI/tech inspection, and an
  independent startup probe that observes production-path SDI displacement
  and tech-timer behavior.
- Canonical replay inputs now exercise vertical axes and trigger edges and
  require observed SDI and tech-window state.

The exact first-primitive behavior and intentional remaining scope are fixed in
[`m4_combat_contract.md`](../product/m4_combat_contract.md).

## Delivered in the missed-tech floor-recovery slice

- A vulnerable 26-tick missed-tech animation now enters explicit `DOWN_WAIT`
  instead of returning directly to idle.
- Ordinary inputs select neutral getup with up or a fresh shield edge, getup
  roll with left/right, and a two-sided 6% floor attack with either attack
  key. Inactive down-wait automatically selects neutral getup after the
  data-defined placeholder timeout.
- Data-defined 30-tick neutral getup, 35-tick roll, and 49-tick floor attack,
  with exact 23-, 19-, and 26-tick invulnerability windows.
- The floor attack covers front frames 17–19 and back frames 24–26, mirrors
  with facing, uses the production one-hit mask/hitlag/launch path, and is
  vulnerable when its invulnerability expires.
- State schema 8/save format 7 and content schema 8 encode the four new action
  semantics and recovery data without increasing the fixed 569-byte save.
- Native verification covers all options, timing boundaries, front/back hits,
  invalid content, and mid-roll save/load continuation. The independent
  browser startup probe exercises knockdown, down-wait, all three options, and
  both attack phases.
- The browser now renders prone states, labels every getup action, draws the
  floor-attack hitbox, and retains the dashed invulnerability indicator.

## Delivered in the first shield slice

- A frame-1 full-density grounded shield using the existing normalized analog
  triggers, while preserving the trigger-edge tech-window contract.
- Melee-style run shield stop with retained momentum and traction, plus an
  explicit oracle that initial dash itself cannot enter shield.
- Data-defined 60 HP, 0.28/tick hold depletion, 0.07/tick regeneration,
  eight-tick minimum hold, 15-tick release, and jump out of shield/release.
- Physical block resolution with zero percent/launch, ordinary hitlag,
  damage-scaled shield health/stun, Melee-style attacker/defender pushback, and
  shield-stun continuation.
- A four-tick physical powershield window with no shield damage, ordinary
  hitlag/stun, larger Melee defender pushback, and inspectable result state.
- Melee-style physical powershield canceling: release by shield-stun end,
  reject attack on shield-drop frame 1, and accept a fresh ground-attack edge
  on frame 2. Ordinary blocks retain all 15 release ticks.
- Complete shield-break flight, forced landing, down/stand phases,
  damage-dependent vulnerable stun, fresh-input mash reduction, interruption,
  and 30-HP recovery supersede the former grounded placeholder lockout.
- Browser shield bubbles, health/stun/powershield diagnostics, hold-to-shield
  controls on `G` and `.`/Numpad `1`, independent normal-block/powershield and
  shield-break startup probes, plus a prone phase and orbiting-star
  `MASH · Nf` countdown.
- Mid-shield-hitlag save/load with equal future hashes and focused shield
  invariants inside the 20,000-tick combat verifier.

## Delivered in the solid-surface tech slice

- Stage schema 2 adds one data-defined raised block that is solid on its top,
  sides, and underside while retaining traversable floor clearance below it.
- Content/fighter schema 9 defines wall-tech, wall-tech-jump, ceiling-tech,
  missed-impact reflection, action duration, stall, and speed values.
- A tumbling side impact with an open window enters wall tech, faces away,
  clears reaction state, stalls three ticks, then moves away; held up or a
  fresh jump edge selects the upward wall-tech jump.
- A tumbling underside impact with an open window enters ceiling tech, clears
  reaction state, zeros vertical motion, and uses horizontal stick-scaled
  velocity.
- Missing either surface window reflects the surface-normal motion, applies
  the 0.8 data coefficient to both components, and preserves tumble and
  remaining hitstun.
- Successful surface techs share the exact 20-tick recovery-invulnerability
  contract. State schema 9/save format 8 adds only canonical action/support
  semantics, so the save remains 569 bytes.
- Native verification now covers default-attack entry, positive and negative
  wall/ceiling outcomes, exact wall stall/release, facing, launch,
  invulnerability, invalid geometry, and seven solid-geometry movement checks,
  including mirrored upper-corner inward-drift penetration regressions.
- Browser view schema 7 renders the block and labels all five actions. Its
  startup readiness probe reaches `WALL_TECH_JUMP` through ordinary movement,
  strong attack, trigger, and up inputs on the default content.

## Delivered in the air-dodge and momentum-landing slice

- Content/fighter schema 10 defines fixed directional air-dodge speed,
  two-axis dead-zone behavior, per-tick decay, special-fall drift mobility,
  total duration, exact invulnerability window, and special-landing lag.
- A fresh airborne trigger edge enters `AIR_DODGE`; neutral input zeros both
  velocities while directional input receives a deterministic normalized
  fixed-speed vector. Facing remains locked and held input cannot retrigger.
- The default dodge decays both motion components for 49 ticks, rejects hits
  on action ticks 3–28, and then enters `FALL_SPECIAL` with gravity, fast fall,
  limited drift, ledge catch, and ordinary air-action lockout.
- Floor, pass-through-platform, and solid-top contact from the dodge or
  helpless fall enters ten-tick `SPECIAL_LANDING`, preserves horizontal
  momentum, and slides under fighter traction. Downward air dodge correctly
  lands on a pass-through platform rather than invoking ordinary drop-through.
- The production ordinary-input path performs a first-airborne-frame short-hop
  air dodge, diagonal floor wavedash, and platform waveland. Neutral input,
  held trigger, late vulnerability, and exact landing-lag boundaries provide
  negative cases.
- State schema 10/save format 9 adds only the three canonical action semantics;
  the complete save remains 569 bytes. Mid-air-dodge save/load produces equal
  continuation hashes.
- Browser view schema 8 labels all three actions, displays the action timer,
  retains the exact invulnerability ring, and runs an independent startup
  probe through `AIR_DODGE`, `FALL_SPECIAL`, `SPECIAL_LANDING`, and continued
  slide.
- The shared replay trace observes both air dodge and special landing before
  encoding. Registry rows 45 (short hop air dodge) and 60 (wavedash) advance
  to `playable`.

## Delivered in the aerial landing and L-cancel slice

- Content/fighter schema 11 defines one original aerial hitbox, damage, launch,
  startup/active/recovery/hitlag phases, landing-lag-active window, ordinary
  landing lag, exact seven-frame L-cancel window, and fixed divisor.
- Airborne light attack enters `AERIAL_ATTACK` while ordinary drift, gravity,
  and fast fall continue. Its hitbox resolves through the existing production
  ownership, damage, hitlag, launch, hitstun, and one-hit-mask path.
- Landing outside action ticks 4–24 auto-cancels into generic `LANDING`.
  Landing inside that window enters 12-tick `AERIAL_LANDING`, or six-tick
  `L_CANCEL_LANDING` when the independent trigger age is 0–6; age 7 is the
  exact negative boundary.
- State schema 11/save format 10 serializes the independent trigger-age byte
  for each player. The complete save is 573 bytes, and mid-aerial save/load
  produces equal canonical hashes.
- Native oracles cover early auto-cancel, normal and reduced lag, every timer
  boundary, invalid data, aerial hitlag/resume/single-hit behavior, and a
  focused per-tick-hash replay of the complete SHFFL route.
- Browser view schema 9 labels all three actions and exposes trigger age and
  eligibility. Its startup probe compares auto-cancel, 12-tick normal landing,
  six-tick L-cancel landing, and timer ages 0–6 versus 7.
- Registry rows 2 (auto-canceling), 29 (L-cancelling), and 46 (short hop fast
  fall l-cancel) advance to `playable`.

## Delivered in the grounded dodge and roll slice

- Content/fighter schema 12 defines original placeholder forward- and
  backward-roll speeds and durations, one shared half-open movement window,
  one shared roll-invulnerability window, and independent spot-dodge duration
  and invulnerability data.
- A fresh full horizontal input with a held or newly pressed trigger enters
  forward or backward roll relative to the fighter's fixed facing. Fresh down
  plus trigger enters spot dodge and wins over a simultaneous horizontal
  input. Held direction alone cannot retrigger an option.
- The placeholder forward roll lasts 31 ticks at `9/50` Q16 units/tick, the
  backward roll lasts 35 ticks at `4/25` Q16 units/tick, and both move only on
  action ticks `[3, 20)` and reject hits on `[4, 17)`. Spot dodge lasts 25
  ticks and rejects hits on `[3, 16)`.
- Rolls preserve facing, stop safely against solid sides, and become airborne
  when their authored motion crosses a support edge.
- State schema 12/save format 11 adds one canonical fresh-down history byte
  per player. The complete stream is 577 bytes, and a mid-spot-dodge
  round-trip produces identical immediate and future hashes.
- Native movement and combat oracles cover neutral-shield and held-down
  negatives, shield-held flick entry, exact duration/distance/invulnerability
  boundaries, wall and edge collision, hit rejection inside each dodge type,
  and hit acceptance immediately before and after spot-dodge invulnerability.
- Browser view schema 10 labels all three actions, retains the derived
  invulnerability ring, exposes ordinary keyboard recipes, and requires an
  independent grounded-dodge startup probe before readiness.
- The shared 180-tick replay now deliberately observes both a grounded roll
  and spot dodge before encoding.

## Delivered in the strong-aerial L-cancel testability slice

- A fresh strong-attack edge while airborne enters
  `STRONG_AERIAL_ATTACK`. It reuses the existing strong attack's startup,
  active, recovery, pink hitbox, 12% damage, launch, and hitlag data through
  the production ownership path; attacker hitlag resumes into the airborne
  action.
- Landing while that action is active always enters a locked 30-tick
  `STRONG_AERIAL_LANDING`. A trigger age of 0–6 instead enters the locked
  15-tick `STRONG_L_CANCEL_LANDING`, making both the failure and success easy
  to count.
- Content/fighter schema 13 adds the independently validated strong landing-lag
  value. State schema 13/save format 12 adds the three action semantics without
  changing the 577-byte stream; inspection schema 12 and browser view schema
  11 expose and label the new outcomes.
- The browser draws a large red missed-L-cancel or green successful-L-cancel
  banner, matching fighter ring, and live remaining-frame counter for light
  and strong aerial landings.
- Solid-top and underside collision sweeps now test the fighter's complete
  horizontal body extent. Mirrored deterministic regressions prevent inward
  drift from entering either raised-block upper corner.
- Native oracles cover strong-aerial entry/hit/resume, locked 30/15-tick
  landing outcomes, invalid data, and both collision corners; the independent
  browser startup probe exercises both strong landing paths before readiness.

## Delivered in the stock, respawn, and result slice

- `pf_sim_config` and the compatibility identity now carry configurable stock
  count, respawn delay, and respawn-invulnerability duration. Defaults are
  four stocks, 60 wait ticks, and 120 invulnerable ticks; stock count zero is
  the explicit unlimited-stock practice mode.
- Blast-boundary crossings consume stock, enter canonical `RESPAWN_WAIT`, and
  either respawn with per-life state reset and hitbox-rejecting
  invulnerability or enter `ELIMINATED` on the final stock.
- The last surviving team terminates the match with a deterministic winner
  mask. Simultaneous final-stock KOs start all players at one stock and 300%;
  a repeated simultaneous sudden-death KO resolves to the lowest port/team.
- That slice introduced ABI 3, state schema 14/save format 13, RL schema 3,
  and browser view schema 12 for the rule/runtime state; the active journal
  slice supersedes them with ABI 4, state schema 15/save format 14, RL schema
  4, and browser view schema 13. The shield-break slice supersedes those state
  and presentation versions with state schema 16/save format 15, content and
  fighter schema 14, inspection schema 14, and browser view schema 14.
  The ledge-invulnerability slice supersedes the canonical state and content
  versions with state schema 17/save format 16 and content/fighter schema 15.
  The small-step-forward-smash slice advances content/fighter schema to 16
  for its hashed, validated three-tick input window without changing state.
  The drop-cancel slice advances content/fighter schema to 17 for its hashed,
  validated snap distance and nine-tick platform pass window, again without
  changing canonical state. The V-cancel slice advances content/fighter schema
  to 18 for its hashed launch scale and input window while reusing the existing
  trigger-age and tech-lockout state. The shield-platform-drop slice advances
  content/fighter schema to 19 for its hashed reduced-down threshold while
  reusing the existing shield, support, airborne, and platform-pass state.
  The planking slice advances state schema to 18/save format 17,
  content/fighter schema to 20, and inspection schema to 15 for a canonical
  29-tick disabled-regrab timer and exact timer inspection.
  The grab slice advances state schema to 19/save format 18,
  content/fighter schema to 21, inspection schema to 16, and browser view
  schema to 15 for reciprocal capture links, escape timing, grab geometry,
  and browser observability. The directional-throw slice advances state schema
  to 20/save format 19, content/fighter schema to 22, inspection schema to 17,
  and browser view schema to 16 for four authored throws, fail-closed throw
  semantics, typed events, and browser observability. The boost-grab slice
  advances state schema to 21/save format 20, content/fighter schema to 23,
  inspection schema to 18, and browser view schema to 17 for the production
  dash attack, its fail-closed cancel semantics, and readiness evidence.
  The jab-cancel slice advances state schema to 22/save format 21,
  content/fighter schema to 24, inspection schema to 19, and browser view
  schema to 18 for the production two-hit jab decision, fail-closed timing,
  typed final hit, and readiness evidence.
  The jab-reset slice advances state schema to 23/save format 22,
  content/fighter schema to 25, inspection schema to 20, and browser view
  schema to 19 for reset-bound and forced-getup reaction semantics. The
  double-jump-cancel slice advances state schema to 24/save format 23,
  content/fighter schema to 26, inspection schema to 21, and browser view
  schema to 20 for the delayed-air-jump action, authored cancellation window,
  vertical-momentum rule, and readiness evidence. The double-jump-cancel-counter
  slice advances state schema to 25/save format 24, content/fighter schema to
  27, inspection schema to 22, and browser view schema to 21 for authored
  knockback-based armor, preserved delayed-action hitlag resume, and readiness
  evidence. The Relay Rod slice advances state schema to 26/save format 25,
  content schema to 28 with fighter schema 27 and item schema 1, inspection
  schema to 23, and browser view schema to 22 for fixed-capacity item state,
  actions, events, and readiness evidence. The jump-cancelling slice advances
  state schema to 27/save format 26 and browser view schema to 23 for the
  fail-closed full-up jump-squat attack route and readiness evidence, without
  changing the content, inspection, observation, RL, or payload layouts. The
  Pulse Bolt slice advances state schema to 28/save format 27, content schema
  to 29 with projectile schema 1, inspection schema to 24, browser view schema
  to 24, input schema to 4, observation schema to 4, RL schema to 6, and compact
  observation schema to 5 for the fixed projectile, actions, events,
  powershield reflection, short-hop-laser readiness, and cross-surface state.
  The Prism Burst slice advances state schema to 29/save format 28, content
  schema to 30 with reflector schema 1, inspection schema to 25, and browser
  view schema to 25 for grounded/aerial reflector actions, downward physical
  launch, active-box projectile reflection, Shine-spike readiness, and
  fail-closed action semantics. Observation, RL, and byte layouts do not
  change. The Arc Reservoir slice advances state schema to 30/save format 29,
  content schema to 31 with charge schema 1, inspection and browser view
  schema to 26, observation schema to 5, RL schema to 7, and compact
  observation schema to 6 for canonical charge ticks, three grounded actions,
  storage cancel/resume/release semantics, readiness evidence, and four
  appended compact values. The Moonwalk slice advances state schema to
  31/save format 30, content schema to 32 with fighter schema 28, and
  inspection/browser view schema to 27 for the two authored grounded actions,
  exact two-tick shallow-back setup, facing-preserving reverse slide, and
  fail-closed action timing. Observation, RL, payload, save-size, and browser
  layout counts do not change. The Teeter-cancel slice advances state schema
  to 32/save format 31, content schema to 33 with fighter schema 29, and
  inspection/browser view schema to 28 for the explicit edge state, authored
  snap distance/duration, and legal standing-action cancels; its payload,
  save-size, observation, RL, and browser layout counts also do not change.
  The Stage-humping slice advances state schema to 33/save format 32, content
  schema to 34 with fighter schema 30, and inspection/browser view schema to
  29 for explicit one-tick crouch-step timing and release-gated repetition;
  those same layout counts remain unchanged. The Taunt-cancel slice advances
  state schema to 34/save format 33, content schema to 35 with fighter schema
  31, input schema to 5, and inspection/browser view schema to 30 for the
  authored grounded action, locked recovery, retained dash momentum, and
  support-edge cancellation. The Scar-Jump slice advances state schema to
  35/save format 34, content schema to 36 with fighter schema 32, and
  inspection/browser view schema to 31 for the authored normal-wall-jump
  launch, action window, brief invulnerability, and preserved air jump; those
  layout counts again remain unchanged. The Vector-Ascent slice advances state
  schema to 36/save format 35, content schema to 37 with recovery schema 1,
  inspection schema to 32, observation schema to 6, RL schema to 8, compact
  observation schema to 7, and browser view schema to 33 for the new action,
  once-per-airtime byte, and visible recovery resource.
  The pummel slice advances state schema to 37/save format 36, content schema
  to 38 with fighter schema 33, inspection schema to 33, and browser view
  schema to 34 for the data-defined action and typed event; the 554-byte
  payload, 694-byte save, 396-value browser view, input, observation, RL, and
  compact layouts remain unchanged.
  The crouch-cancel slice advances state schema to 38/save format 37, content
  schema to 39 with fighter schema 34, inspection schema to 34, and browser
  view schema to 35 for the grounded/resulting-damage qualification, two
  reaction scales, derived tumble, and typed event flag; those same payload,
  save, browser, input, observation, RL, and compact layouts remain unchanged.
  The victim-weight slice advances content schema to 40 with fighter schema 35
  for one hashed Q16.16 field. State schema 38/save format 37, inspection
  schema 34, browser view schema 35, and every serialized layout remain
  unchanged because default weight 1.0 is an identity target modifier.
  The directional-ground-attack slice advances state schema to 39/save format
  38, content schema to 41 with fighter schema 36, inspection schema to 35, and
  browser view schema to 36 for `UP_ATTACK`/`DOWN_ATTACK`, vertical-dominant
  light-input arbitration, two embedded attack definitions, signed two-axis
  launch, hitlag resume, and powershield-cancel routing. The 554-byte payload,
  694-byte save, 396-value browser view, input, observation, RL, compact, and
  66-value compact layouts remain unchanged.
  Config/identity schema 2 remains current. At that slice, the canonical save
  was 694 bytes; the later grounded-normal/smash-charge, analog-light-shield,
  and shield-geometry slices below supersede it with the current 726-byte
  format.
- A 24-invariant match oracle covers configuration bounds, stock loss,
  respawn/invulnerability boundaries, hit rejection and expiry, mid-respawn
  save/load continuation, final-stock result, sudden death, and 2v2 team
  result.
- The browser HUD shows stocks and both timers, fades respawning/eliminated
  fighters, draws respawn invulnerability, pauses on the result, turns Reset
  into Rematch, and requires an independent ordinary-input KO/respawn startup
  probe.

## Delivered in the deterministic event-journal slice

- ABI 4 adds a fixed 16-entry `pf_tick_result` journal with 32-byte typed
  records for hits, shield blocks, powershields, shield breaks, KOs, respawns,
  sudden death, match results, forfeits, and time limits.
- Every event carries its processed input tick, match-monotonic sequence,
  source/target slots, Q16.16 value and velocity, flags, and type-specific
  detail. Stable player-slot production order and match-resolution-last order
  are deterministic.
- Only the existing sequence authority is canonical. Per-tick arrays are
  caller-owned scratch output, so state remains bounded while save/load and
  rollback re-simulation reproduce byte-identical event records.
- State schema 15/save format 14 and magic `PFSAVE14` failed closed on the new
  journal semantics without changing the 463-byte payload or 603-byte complete
  checkpoint. Replay API/verification schema 2 and RL/transition schema 4
  carry the enlarged tick-result ABI.
- The 180-tick native/WebAssembly corpus now hashes every event under the
  `PFEVT001` domain in addition to its per-tick state hashes. Replay wire
  format 1 remains byte-compatible at 31,295 bytes and deterministically
  re-emits, rather than stores, event payloads.
- Browser view schema 13 exposed the current tick's 16 slots. The client keeps
  only the newest ten for presentation, clears that history after a rewind or
  reset, and labels hit/shield/KO/respawn/sudden-death/result events by
  canonical sequence.

## Delivered in the complete shield-break slice

- Zero shield HP from either a physical hit or held-shield depletion launches
  the fighter upward in locked `SHIELD_BREAK`, applies gravity without drift
  or fast fall, and forces the first legal surface contact.
- Landing enters data-defined `SHIELD_BREAK_DOWN` and
  `SHIELD_BREAK_STAND` phases before vulnerable `SHIELD_BREAK_STUN`.
  Flight/down/stand reject hitboxes; an ordinary flinching hit interrupts
  stun.
- Default stun uses Melee's `max(90, 490 - floor(percent))` duration. Fresh
  buttons, trigger presses, full-horizontal flicks, and down flicks remove
  three extra ticks while held inputs do not repeat.
- Natural expiry and interruption restore 30 shield HP. A lost moving support
  returns any grounded break phase to locked fall.
- Hit-caused and hold-depletion routes both emit canonical shield-break events;
  the latter uses the system source and its actual depleted health/launch.
- State schema 16/save format 15 and `PFSAVE15` add only the three action
  semantics, retaining the 463-byte payload and 603-byte checkpoint. Content
  and fighter schema 14 validate launch/timing/mash data; inspection and
  browser view schema 14 expose and label the result.
- Browser readiness now includes an ordinary-input full-depletion probe through
  launch, down, stand, stun, fresh-versus-held mash, and recovery. The live
  view renders down prone and stun with orbiting stars plus a remaining-frame
  mash counter.
- Registry row 40, Shield break combo, advances to `playable` with a focused
  deterministic punish and mid-stun save/load continuation oracle.

## Delivered in the ledge-invulnerability foundation

- A legal catch now refreshes a data-defined 37-tick timer: the seven-tick
  catch transition plus 30 additional ticks, matching the documented Melee
  `CliffCatch` rule used by
  [ledgestalls](https://www.ssbwiki.com/Ledgestall).
- The timer survives release, ledge jump, climb, aerial movement, and landing
  until it naturally reaches zero. It rejects production hit ownership and is
  reflected by the existing inspection/browser invulnerability marker.
- State schema 17/save format 16 and `PFSAVE16` serialize one remaining timer
  per player, expanding the checkpoint from 603 to 611 bytes. Content/fighter
  schema 15 hashes and validates the configured duration.
- Native movement coverage proves invalid-data rejection, exact expiry,
  post-release carry, hit rejection while nonzero, hit acceptance on expiry,
  and save/load continuation. This is a foundation for edge hopping, edge
  dashing, planking, and ledge-option completion; it does not promote those
  registry rows by itself.

## Delivered in the ledge-regrab and planking slice

- Every ordinary release from `LEDGE_HANG` now starts a separate, data-defined
  29-tick disabled-regrab period. Remaining ticks 28 through 1 reject an
  otherwise legal catch; tick 0 permits the normal facing, geometry, velocity,
  and single-occupancy catch checks.
- The lockout is not the pass-through-platform timer, so it cannot suppress
  floor or platform landings. Reset, KO, and respawn clear it.
- A narrow fixture tunes the ordinary double-jump arc to return on the first
  legal catch tick. Three consecutive drop/jump/regrab cycles refresh the
  37-tick ledge invulnerability against an active responding jab; fast-falling
  on the last two ticks misses the refresh and accepts that punish.
- The native oracle saves immediately after release and compares every future
  hash through all three cycles. Browser startup repeats both routes, reports
  `planking_probe=1`, and restores default content.
- State schema 18/save format 17 and `PFSAVE17` add one remaining regrab
  lockout per player, expanding the checkpoint from 611 to 619 bytes.
  Content/fighter schema 20 hashes and validates the 29-tick default;
  inspection schema 15 exposes both exact ledge timers. Registry row 36,
  Planking, advances to `playable`.

## Delivered in the jump-canceled-grab slice

- Fresh light attack while shield is held selects a four-startup/two-active/
  ten-recovery standing grab from ordinary grounded states and jump squat.
  Shield cannot block capture; invulnerability and same-team ownership reject
  it, and stable controller-port order resolves competing attackers.
- Capture enters reciprocal `GRAB_HOLD`/`GRABBED` state and tethers the victim.
  The 30-to-90-tick percent-scaled escape timer counts down naturally, while
  fresh button, full-horizontal, or full-down edges remove three extra ticks and held inputs do not
  repeat. Escape clears both links and emits an eight-tick `GRAB_RELEASE`.
- Initial dash cannot grab directly. Dash into jump squat and light-plus-shield
  on the next tick enters standing grab while retaining positive dash momentum;
  the same input after takeoff remains an ordinary airborne action.
- The native oracle covers exact phases, shielded capture, spot-dodge and
  same-team rejection, lower-port priority, percent scaling/cap, natural/mash
  escape, direct-dash and airborne negatives, typed grab/escape events,
  invalid content, and mid-hold save/load equality. Browser startup
  repeats the route and negatives before readiness, then exposes the cyan
  grabbox, reciprocal links, action labels, and escape countdown.
- State schema 19/save format 18 and `PFSAVE18` add one 16-bit escape timer and
  two one-byte link slots per player, expanding the checkpoint from 619 to 635
  bytes. Content/fighter schema 21 and inspection schema 16 carry the authored
  grab data; browser view schema 15 carries the observable grab state. Registry
  row 26, Jump-canceled grab, advances to `playable`.

## Delivered in the directional-throw and chain-grab slice

- A fresh light or strong attack plus full direction during `GRAB_HOLD`
  selects data-defined forward, back, up, or down throw. Horizontal is relative
  to facing and wins a diagonal tie; only a strictly dominant vertical input
  selects up/down, while neutral or reduced input remains in hold.
- Each throw has independent damage, signed base/per-percent launch, exact
  release tick, hitlag, and recovery. Reciprocal links persist through startup,
  clear atomically at release, and the shared hit-reaction path supplies
  hitstun/tumble, SDI/DI, attribution, hitlag, and one typed throw event.
- The native oracle checks all four exact startup/release/recovery routes,
  horizontal diagonal-tie and vertical-dominance selection, neutral/reduced
  rejection, a three-down-throw chain with two legal low-percent regrabs, an
  earliest regrab whiff at 96% under outward DI, invalid authored timing, and a
  mid-chain save/load continuation with equal future hashes and events.
- Browser startup repeats all four throws, the complete two-regrab chain, and
  the neutral-input negative. The live adapter labels all four throw actions,
  renders the typed damage/launch event, and confirms reciprocal links clear.
- State schema 20/save format 19 and `PFSAVE19` retain the 635-byte checkpoint
  while making throw action, startup-link, release, hitlag-resume, and recovery
  semantics fail closed. Content/fighter schema 22 carries the throw data,
  inspection schema 17 identifies the contract, and browser view schema 16
  exposes it. Registry row 6, Chain grab, advances to `playable`.

## Delivered in the boost-grab slice

- A fresh light-attack edge from `RUN` now enters a production dash attack
  with independently authored speed, hitbox, damage, launch, startup, active,
  recovery, and hitlag data. The default has four startup, three active, 12
  recovery, and five hitlag ticks and emits the ordinary typed hit event with
  `DASH_ATTACK` identity.
- Stored dash-attack ticks 1–3 accept a fresh shield while light remains held,
  or a fresh light edge while shield remains held, and enter the existing
  standing grab without discarding faster dash-attack momentum. These are
  action frames 2–4; the initiation frame and every later frame are excluded.
  Light plus shield together from `RUN` remains the ordinary dash grab.
- The native oracle compares ordinary and boosted velocity/range, requires an
  ordinary whiff and boosted capture, rejects the late cancel, proves the dash
  attack's first active hit and typed identity, rejects invalid authored data,
  and compares every future hash and event after a mid-route save/load.
- Browser startup repeats the ordinary whiff, boosted capture, late negative,
  and production dash-attack hit before readiness. The live adapter labels the
  new action and reports `boost_grab_probe=1`.
- State schema 21/save format 20 and `PFSAVE20` retain the 635-byte checkpoint
  while making dash-attack entry, hitlag resume, and boost-grab cancellation
  fail closed. Content/fighter schema 23, inspection schema 18, and browser
  view schema 17 identify the new contract. Registry row 4, Boost grab,
  advances from `planned` to `playable`.

## Delivered in the jab-cancel slice

- The production first jab now exposes a hashed, validated stored-action-tick
  4–7 decision window. A fresh trigger selects the existing `SHIELD` state on
  hit or whiff, fresh light selects the independently authored `JAB_FINAL`,
  and neutral input completes the first jab without hidden buffer state.
- `JAB_FINAL` has independent hitbox, 7% damage, signed launch growth,
  two-startup/two-active/ten-recovery timing, four hitlag ticks, and typed hit
  identity. Early-held shield and the first post-window frame are rejected.
- The native oracle proves exact begin/end boundaries, hit/whiff parity, both
  timing negatives, final-hit damage/identity, invalid data, and mid-window
  save/load with equal future hashes and events. Browser startup repeats every
  route, reports `jab_cancel_probe=1`, and labels `JAB FINAL`.
- State schema 22/save format 21 and `PFSAVE21` retain the 635-byte
  checkpoint while making final-jab action, timing, hitlag-resume, and choice
  semantics fail closed. Content/fighter schema 24, inspection schema 19, and
  browser view schema 18 identify the contract. Registry row 22, Jab cancel,
  advances from `planned` to `playable`.

## Delivered in the jab-reset slice

- A physical hit against vulnerable `DOWN_WAIT` or an already-resetting target
  now qualifies for jab reset only when its authored per-hit damage is at most
  7% and its computed hitstun is at most 12 ticks. The default 6% jab reaches
  the exact 12-hitstun route without a technique-only flag or buffer.
- A qualifying hit clears horizontal launch, applies the fixed low `1/10`
  unit-per-tick bounce, resumes into `RESET_BOUND` after ordinary hitlag, and
  uses the existing action timer for exactly 12 bound ticks. Ground contact at
  expiry enters 30 vulnerable `FORCED_GETUP` ticks; an airborne target instead
  recovers into `AIRBORNE` and may act.
- Ordinary same-tick getup input resolves before combat, so its existing
  invulnerability avoids the reset. Existing hitlag SDI/ASDI and final-hitlag
  DI remain live; two SDI pulses plus ASDI can keep the target airborne at
  bound expiry. Over-7% hits and 13-hitstun tumble remain ordinary reactions.
- The native oracle proves both inclusive limits, exact action timing and input
  lock, same-tick getup avoidance, airborne SDI escape into aerial attack,
  invalid data, and a mid-bound save/load with 64 equal future hashes and
  events through a real forced-getup punish. Browser startup repeats the
  positive, getup, over-damage, and SDI routes, reports
  `jab_reset_probe=1`, and labels both new actions.
- State schema 23/save format 22 and `PFSAVE22` retain the 635-byte checkpoint
  while making `RESET_BOUND`, `FORCED_GETUP`, hitlag resume, action timing, and
  grounded/airborne expiry semantics fail closed. Content/fighter schema 25,
  inspection schema 20, and browser view schema 19 identify the contract.
  Registry row 23, Jab reset, advances from `primitive-ready` to `playable`.

## Delivered in the instant-double-jump slice

- A released first jump followed by a fresh jump edge on the first legal
  airborne frame consumes exactly one configured air jump and applies the
  exact data-defined double-jump velocity before ordinary gravity.
- A jump edge on the takeoff tick cannot consume the air jump, holding one jump
  input through takeoff cannot repeat it, and an exhausted air jump cannot be
  reused.
- The focused movement oracle checks exact first-frame position/velocity and a
  mid-IDJ save/load continuation with matching future canonical hashes.
- Browser readiness runs both the positive route and the held-input negative
  route. The live state card already exposes the remaining-air-jump counter,
  and the playtest text gives a two-key keyboard recipe.
- Registry row 21, Instant double jump, advances from `planned` to `playable`;
  owner execution and the remaining acceptance evidence are still required
  before `verified`.

## Delivered in the double-jump-cancel slice

- A legal air jump enters `DELAYED_AIR_JUMP` for the default six-tick
  half-open action window `[0, 6)`. A fresh light or strong aerial in that
  window cancels remaining upward velocity before ordinary gravity and motion;
  the first late aerial preserves the full rise.
- Simultaneous fresh jump and attack while ordinarily airborne gives the
  attack priority without consuming the air jump. Setting the authored window
  to zero disables the delayed state and retains the ordinary air-jump route.
- The focused movement oracle proves entry, both early aerials, the exact last
  legal and first late boundaries, earlier landing after cancellation, the
  simultaneous-input negative, invalid content, disabled content, and a
  mid-window 635-byte save/load continuation with equal future hashes through
  landing.
- Browser readiness repeats the early, late, landing-time, and simultaneous
  routes and reports `double_jump_cancel_probe=1`; the live adapter labels the
  delayed action and exposes its action timer and remaining air jumps.
- State schema 24/save format 23 and `PFSAVE23` make the action and timing
  semantics fail closed without changing the 635-byte checkpoint.
  Content/fighter schema 26 hashes the authored window, inspection schema 21
  identifies the state contract, and browser view schema 20 identifies the
  presentation contract. Registry row 12 advances from `planned` to
  `playable`; owner execution and remaining acceptance evidence are still
  required before `verified`.

## Delivered in the double-jump-cancel-counter slice

- A physical hit during `DELAYED_AIR_JUMP` now qualifies against the default
  inclusive 20-hitstun armor threshold. It still applies damage, attribution,
  and ordinary hitlag, but emits zero launch, preserves the exact double-jump
  trajectory/action tick, and resumes the delayed action without hitstun or
  tumble.
- The resumed defender may immediately use the existing aerial cancel and land
  a real counter-hit. No counter-only action, input, event flag, or canonical
  mutable field was introduced.
- The focused combat oracle proves the exact 16-hitstun boundary, disabled and
  invalid content, weak armored contact, frozen trajectory, immediate counter,
  a first-late ordinary launch, and a 34-hitstun strong-aerial armor break. It
  also saves the mid-hitlag state in the unchanged 635-byte stream and compares
  all future hashes and typed events after load.
- Browser readiness repeats the armored hit/counter, late-window failure, and
  strong-hit failure before restoring default content and reports
  `double_jump_cancel_counter_probe=1`.
- State schema 25/save format 24 and `PFSAVE24` make armor qualification,
  zero-launch reaction state, preserved delayed-action timing, and hitlag
  resume fail closed. Content/fighter schema 27 hashes the armor threshold,
  inspection schema 22 identifies the contract, and browser view schema 21
  identifies its readiness evidence. Registry row 13 advances from `planned`
  to `playable`; owner execution and remaining cross-target acceptance evidence
  are still required before `verified`.

## Delivered in the edge-hop route

- The complete route now uses ordinary match input: down releases a legal ledge
  hang, a fresh inward jump on the next tick consumes the configured air jump,
  and either aerial attack can follow without changing airborne facing.
- The focused movement oracle compares neutral hang, the positive route, an
  exhausted second-jump rejection, retained ledge invulnerability, and a
  mid-route save/load continuation with matching future canonical hashes.
- Browser readiness performs the same route and negative checks before exposing
  the playtest. The live air-jump counter and invulnerability marker make both
  state changes observable without debug-state mutation.
- Registry row 16, Edge hopping, advances from `planned` to `playable`; complete
  rollback/replay and owner evidence are still required before `verified`.

## Delivered in the edge-dash route

- The complete ordinary-input route now composes the existing ledge jump,
  37-tick catch invulnerability, inward directional air dodge, and ten-tick
  momentum-preserving special landing. No edge-dash-only state or debug setup
  was added.
- The first actionable ground frame still carries ledge invulnerability and can
  immediately enter the production light attack. Waiting on ledge until the
  timer expires produces the same landing route without that overlap.
- The focused movement oracle checks ledge-jump positioning, air-dodge entry,
  inward landing momentum, exact locked landing ticks, actionable overlap,
  attack follow-up, expired-window rejection, and mid-route save/load future
  hashes.
- Browser readiness runs both the positive route and expired-window negative
  route. Registry row 15, Edge dashing, advances from `planned` to `playable`;
  complete rollback/replay and owner evidence remain before `verified`.

## Delivered in the fox-trot route

- Repeated full-direction tap/release input now has an explicit ordinary-match
  route: every fresh same-direction edge restarts tick 1 of `INITIAL DASH`,
  while the release clears the dash edge and preserves a short traction slide.
- The focused movement oracle executes four consecutive bursts, checks exact
  action timer, direction, facing, velocity, and forward travel, then proves a
  held direction reaches `RUN` and a reduced-magnitude re-entry only `WALK`s.
- A save taken between bursts restores into a second simulation and every
  future dash/release tick produces the same canonical hash. Browser readiness
  repeats the positive rhythm and both negative routes.
- Registry row 17, Fox-trotting, advances from `planned` to `playable`; owner
  execution and full native/WebAssembly replay evidence remain before
  `verified`.

## Delivered in the pivot route

- A one-tick opposite full input during `INITIAL DASH`, followed by neutral or
  an immediate ground action, now has an explicit production-path pivot
  contract. The route retains the new facing and reversal momentum; omitting
  the action produces the corresponding empty pivot.
- The focused movement oracle checks exact turnaround timing, grounded attack
  entry, facing and residual velocity, a held-reversal continuation, and a
  post-window `RUN TURNAROUND` negative case. A save on the pivot frame loads
  into a second simulation with equal future canonical hashes.
- Browser readiness repeats the attack, empty, held, and post-run routes before
  exposing the interactive loop. Registry row 35, Pivoting, advances from
  `planned` to `playable`; owner execution and complete replay/rollback evidence
  remain before `verified`.

## Delivered in the dash-cancel route

- Superseded by the 2026-08-12 Run-to-RunBrake live qualification below: the
  direct Run-to-Crouch behavior recorded in this historical delivery section
  was project behavior, not the final Melee-equivalent route. Current
  production enters RunBrake first and accepts crouch from RunBrake.
- Down from an unlocked `RUN` now enters `CROUCH` directly instead of
  `RUN BRAKE`, preserving a traction-reduced forward slide and allowing an
  immediate grounded attack. Initial-dash jump cancel and run-to-shield remain
  production routes through the same ordinary input path.
- The focused movement oracle checks jump, crouch, shield, follow-up attack,
  facing and momentum, rejects shield during `INITIAL DASH` and crouch during
  `RUN TURNAROUND`, and proves mid-crouch save/load future-hash equality.
- Browser readiness repeats the positive and negative routes. Registry row 9,
  Dash cancel, advances from `planned` to `playable`; the future grab, special,
  and broader attack set must join this router as those actions land before the
  row can become `verified`.

## Delivered in the dashing-shield route

- A one-tick shield tap from `RUN` now has an explicit production-path
  contract: inherited run momentum slides under ordinary traction through the
  eight-tick minimum hold, then the existing 15-tick `SHIELD RELEASE` returns
  the fighter to an actionable grounded state.
- The focused combat oracle compares the tap against a held shield stop at
  every boundary, including exact position and velocity, final action and
  shield health, an idle no-travel negative case, and mid-route save/load
  future-hash equality.
- Browser readiness repeats the tap, held, and idle routes. Registry row 11,
  Dashing shield, advances from `planned` to `playable`; owner execution plus
  full native/WebAssembly replay and rollback evidence remain before
  `verified`.

## Delivered in the tech-chase route

- A production strong launch now feeds a complete ordinary-input chase: the
  attacker follows the opponent's airborne path, observes either neutral tech
  in place or a right tech roll, continues adjusting spacing, and jabs during
  the vulnerable recovery tail after the exact 20-tick invulnerability ends.
- The focused combat oracle proves both reacting punish outcomes, a
  same-action-tick non-following jab that misses the roll, and canonical
  save/load future-hash equality from the middle of that roll.
- Browser readiness repeats the two reacting routes and the static miss before
  exposing the interactive loop. Registry row 56, Tech-chasing, advances from
  `primitive-ready` to `playable`; owner execution and a broader opponent
  decision policy remain before `verified`.

## Delivered in the small-step-forward-smash route

- Full direction plus the ordinary light-attack edge now enters the production
  strong attack from idle. Delaying light attack through the authored
  three-tick same-direction initial-dash window preserves traveled distance and
  velocity, extending the same strong hitbox without a technique-only state.
- The focused combat oracle proves a standing forward smash misses at a spacing
  where the frame-3 delayed route lands for the authored 12%, validates the
  content window, rejects frame 4 and a missing direction, and checks save/load
  continuation with equal future hashes.
- Browser readiness repeats standing, frame-3, frame-4, and released-direction
  routes. Registry row 47 advances from `planned` to `playable`; owner execution
  and full native/WebAssembly replay and rollback evidence remain before
  `verified`.

## Delivered in the drop-cancel route

- The platform pass-through timer is now nine ticks, and the drop-input tick
  applies the authored nudge and ordinary gravity without also enabling fast
  fall. A light aerial started on the first airborne frame can therefore hit
  before the pass timer expires.
- Attacker hitlag counts down that timer. On the final hitlag tick, the exact
  first-frame route snaps within the validated five-eighths-unit distance and
  enters the ordinary 12-tick `AERIAL_LANDING` on the same platform. No
  technique-only action or mutable flag was added.
- The focused combat oracle proves the hit/snap/landing route, one-tick-late
  connecting and frame-perfect whiff fall-through negatives, invalid content,
  exact landing lag, and save/load future-hash equality. Browser readiness
  repeats the positive and one-tick-late ordinary-input routes.
- Registry row 14, Drop cancel, advances from `planned` to `playable`; owner
  execution plus complete native/WebAssembly replay and rollback evidence
  remain before `verified`.

## Delivered in the V-cancel route

- A full trigger on the collision tick or either of the prior two ticks now
  scales both pending launch components to exactly 95% when the defender is in
  `AIRBORNE`, `FALL_SPECIAL`, or vulnerable `AIR_DODGE` startup. Hitstun and
  tumble remain the ordinary hit's values.
- The qualifying edge must be the edge that opened the existing 40-tick tech
  lockout. Grounded defenders, active aerial attacks, locked hitstun, the exact
  age-3 boundary, and a repeated trigger inside lockout receive ordinary
  launch. No technique-only action or mutable state was added.
- The focused combat oracle covers every age boundary, exclusions, both launch
  components, invalid data, and mid-route save/load with byte-identical events
  and future hashes. Browser readiness compares ordinary, successful,
  attacking, and locked-out routes through default ordinary input.
- Registry row 59, V-cancelling, advances from `planned` to `playable`; owner
  execution plus complete encoded replay/rollback and cross-target evidence
  remain before `verified`.

## Delivered in the shield-platform-drop route

- An already-shielding fighter on pass-through support now drops through when
  down lies in the validated `[12288, 16384)` input band. The transition reuses
  ordinary `AIRBORNE`, the authored nudge, gravity, and the existing nine-tick
  platform-pass timer; it adds no canonical mutable state.
- The lower band edge and `crouch_axis_threshold - 1` are eligible. One value
  below the band remains `SHIELD`, full down enters `SPOT DODGE`, solid floor
  cannot drop, same-tick shield entry only raises shield, and releasing after
  minimum hold remains grounded `SHIELD RELEASE`.
- The focused movement oracle validates both data bounds and all route
  boundaries, then saves while shielded and proves the drop plus 24 future
  hashes are identical after load. Browser readiness repeats successful,
  below-band, and full-down routes through default input.
- Registry row 41, Shield platform dropping, advances from `planned` to
  `playable`; owner execution plus complete encoded replay/rollback and
  cross-target evidence remain before `verified`.

## Delivered in the approach route

- From the default 16-unit neutral separation, ordinary reduced-stick walking
  now supplies a verifier-readable offensive approach rather than a prepared
  close-range fixture.
- Braking just outside the responder's jab reach lets that jab whiff and the
  longer strong attack convert during recovery. Walking too close produces the
  responding negative case: the same jab intercepts the approach first.
- The native and browser probes use the default stage, ordinary input, and the
  existing movement/attack paths. They add no technique-only state or content
  data.
- Registry row 1, Approach, advances from `planned` to `playable`; broader
  approach options, owner execution, and complete encoded replay/rollback
  evidence remain before `verified`.

## Delivered in the spacing route

- The default forward jab reaches 1.8 center-to-center units, while the strong
  attack reaches 2.1. A deterministic responder now jabs first so the tactic is
  demonstrated as a legal exchange rather than a scripted outcome.
- At 1.95 units the jab whiffs and the opponent's ordinary strong counter
  connects during jab recovery. At 1.7 the jab hits before that counter can
  start; at 2.25 both attacks whiff. A safe-tip shield control proves the
  longer hitbox still makes legal contact.
- The focused combat oracle saves after the whiff-counter begins and proves 32
  future hashes after load. Browser readiness reaches close, safe, and far
  bands with reduced-stick walking on the default stage and repeats all three
  exchanges plus the shield control.
- Registry row 49, Spacing, advances from `planned` to `playable`; owner
  execution plus complete encoded replay/rollback evidence remain before
  `verified`.

## Delivered in the platform-sharking route

- A target can remain on the one-way platform while the attacker stands in
  legal floor space below, full hops, and initiates the existing light aerial
  before crossing the platform line. The positive route deals the authored 8%
  and preserves player-0 hit ownership.
- An aerial started immediately after takeoff reaches active frames too low and
  whiffs. Holding the target's trigger instead converts the correctly timed hit
  into an ordinary shield block with zero percent, reduced shield health, no
  powershield, and the typed block event.
- The native oracle saves immediately after aerial startup and proves 32 equal
  future hashes after load. Browser readiness repeats the hit, early whiff, and
  shield routes on the default moving platform, tracking it through ordinary
  floor walking and full airborne steering.
- Registry row 39, Sharking, advances from `planned` to `playable`; upward
  attack variety, broader opponent movement responses, owner execution, and
  complete encoded replay/rollback evidence remain before `verified`.

## Delivered in the cross-up route

- Player 0 can face away in front of a shielding defender, short hop through
  with ordinary drift, and begin the existing light aerial from the rear side.
  Its backward-facing hitbox blocks and the attacker finishes behind while
  preserving the away-facing direction.
- Starting that neutral aerial immediately after takeoff produces active frames
  on the wrong side and whiffs. Repeating the descent timing while facing
  toward the defender blocks but leaves the attacker in front, providing the
  deterministic side/facing control.
- The native oracle saves after rear-side aerial startup and proves 48 equal
  future hashes through the physical shield block and landing. Browser
  readiness walks both players to clear legal space on the default stage, then
  repeats the rear block, early whiff, and front block with ordinary input.
- Registry row 8, Cross-up, advances from `planned` to `playable`; the
  production grab now exists, but a focused shield-grab comparison and owner
  execution remain before complete verification.

## Delivered in the mindgame route

- The same visible reduced-stick approach cue now supports two tested reads.
  Against the jab-first response, the attacker brakes outside reach and strong
  counters recovery. Against held shield, the attacker faces away, short hops
  through, and finishes the rear-side aerial cross-up behind the defender.
- The wrong branches remain ordinary outcomes: strong attack into shield is
  blocked, an immediate neutral aerial whiffs on the wrong side, and the
  forward-facing aerial control blocks while staying in front.
- Browser readiness exposes `mindgame_probe=1` only when the approach, all
  close/safe/far spacing outcomes, shield control, cross-up, early whiff, and
  front-block control pass together. The constituent native ground and aerial
  save/load oracles supply deterministic continuation evidence.
- Registry row 32, Mindgame, advances from `planned` to `playable`; broader
  conditioning history, a focused grab-mixup branch, and owner execution remain
  before `verified`.

## Delivered in the juggling route

- The existing grounded strong attack now serves as an ordinary 12% launcher;
  Player 0 follows the live airborne trajectory, full hops, and connects the
  production 8% light aerial before Player 1 touches a surface.
- Holding launch DI and using a fresh directional air dodge after hitstun makes
  the same active aerial follow-up whiff, leaving the target at the original
  12% and providing an active escape policy rather than a stationary dummy.
- The native oracle saves immediately after the launcher and compares every
  future hash through the successful aerial. Browser readiness walks both
  players into legal default-stage space and repeats the hit and escape routes
  around the moving platform through ordinary input.
- Registry row 24, Juggling, advances from `planned` to `playable`; longer
  percent-dependent chains, stock conversions, broader fighter coverage,
  owner execution, and complete encoded replay/rollback evidence remain before
  `verified`.

## Delivered in the ladder route

- A validated upward-aerial fixture now composes a full hop, three legal 4%
  light aerials, a double jump after hit two, and the ordinary 12% strong
  aerial finisher. The route carries the defender more than two units upward,
  crosses above the pass-through platform, and takes the stock at the upper
  blast line at 24%.
- The defender remains in canonical hitlag or hitstun from first contact
  through the typed KO. Outward DI beginning after the first hit instead makes
  the next active aerial whiff, returns the defender to an actionable state,
  and preserves the stock.
- The native oracle saves after hit two and compares every future hash through
  the finisher. Browser readiness repeats the ladder and escape routes in the
  WebAssembly-facing simulation, then restores default content.
- Registry row 30, Ladder, advances from `planned` to `playable`; broader
  fighter/route coverage, owner execution, and complete encoded
  replay/rollback evidence remain before `verified`.

## Delivered in the kill-confirm route

- A validated technique-support fixture composes a fast, low-launch 6% jab
  into the existing 12% strong finisher. Twenty ordinary buildup jabs establish
  120%; the final jab holds the target in canonical hitlag/hitstun until the
  earliest strong follow-up connects and takes the stock at 138%.
- The percent and DI controls remain real match outcomes. Starting from 0%
  makes the same jab-to-strong route land safely at 18%, while outward DI at
  120% makes the same active finisher whiff and leaves the target at 126%.
- The native oracle saves immediately after the setup hit and compares every
  future hash through the attacker-attributed typed KO. Browser readiness runs
  the conversion and both controls in the WebAssembly-facing simulation, then
  restores default content before exposing the live playtest.
- Registry row 28, Kill confirm, advances from `primitive-ready` to `playable`;
  broader fighter/percent windows, owner execution, and complete encoded
  replay/rollback evidence remain before `verified`.

## Delivered in the zero-to-death route

- The validated fast-jab fixture now starts from exact 0% and chains 21 legal
  six-percent jabs into the ordinary strong finisher for a typed 138% stock
  loss. Each follow-up begins at the attacker's earliest grounded-idle tick,
  while the defender remains in canonical hitlag or hitstun from first contact
  through the KO.
- The declared defense starts outward DI only after the first hit. Its
  hitlag/launch displacement breaks the sequence before the finisher, makes a
  later active jab whiff, returns the defender to an actionable state, and
  preserves the stock.
- The native oracle saves after hit 11 and compares every future hash through
  the KO. Browser readiness runs the uninterrupted conversion and DI escape
  in the WebAssembly-facing simulation, then restores default content before
  the live playtest.
- Registry row 61, Zero-to-death combo, advances from `primitive-ready` to
  `playable`; broader fighter coverage, owner execution, and complete encoded
  replay/rollback evidence remain before `verified`.

## Delivered in the ledge-cancelling route

- A stationary narrow-platform fixture now composes full hop, a descending
  down-right directional air dodge, waveland contact, retained horizontal
  momentum, traction, and the production support-loss transition. The fighter
  lands beside the right edge in `SPECIAL LANDING`, slides past the bound on
  the first recovery tick, and immediately becomes ordinary `AIRBORNE` at
  action tick zero.
- The geometry control repeats the same inputs at platform center and remains
  locked in `SPECIAL LANDING` for action ticks 0–9 before returning to idle.
  No ledge-cancel-only action, state, or content switch was added.
- The native oracle saves on the landing tick and compares every future hash
  through the airborne continuation. Browser readiness repeats the edge and
  center routes in the WebAssembly-facing simulation, then restores default
  content before exposing the live playtest.
- Registry row 31, Ledge-cancelling, advances from `planned` to `playable`;
  broader surfaces/actions, owner execution, and complete encoded
  replay/rollback evidence remain before `verified`.

## Delivered in the Relay Rod item slice

- One original fixed-capacity Relay Rod now supplies data-driven pickup,
  carry, grounded/aerial drop, four directional throws, an airborne hitbox,
  damage/knockback/bounce, pickup lockout, lifetime, despawn, and reset without
  allocation or dynamic entity creation.
- Ordinary input priority composes three registry routes: aerial light plus
  shield drops the held item; a fresh attack during grounded roll frames 0–4
  glide-tosses it while preserving roll momentum; and a fresh attack during
  jump squat cancels takeoff while preserving dash momentum. Spacing, frame-5,
  and first-airborne-frame controls prove the corresponding negative routes.
- `tests/sim/test_m4_item.c` adds 44 focused invariants, all four throw
  directions, typed item events, save/load future equality, encoded replay
  verification, structured and compact RL observation, and despawn/reset.
  `tools/verify_m4_item.sh` makes the result a verifier-readable check.
- State schema 26/save format 25 and `PFSAVE25` expand the canonical payload
  from 495 to 522 bytes and the checkpoint from 635 to 662 bytes. Structured
  observation schema 3 and RL schema 5 expose the fixed item; compact
  observation schema 4 appends eight values for a 56-value vector.
- Browser startup repeats positive and negative bat-drop, glide-toss, and
  jump-cancel-throw routes before readiness. View schema 22 appends exact item
  state/collision data after all prior offsets, renders the Relay Rod and its
  active box, and exposes an ordinary live pickup/throw practice route.
- Registry rows 3, 18, and 25 advance from `planned` to `playable`; cross-target
  evidence and owner execution remain before `verified`.

## Delivered in the jump-cancelling attack slice

- Full up plus a fresh light or strong edge during the production three-tick
  jump squat now selects the existing grounded standing strong attack, cancels
  takeoff, and preserves inherited dash momentum under ordinary traction.
- Neutral input, an up magnitude one unit below the full-input threshold, and
  first-airborne-frame input retain their ordinary jump-squat or aerial routes.
  Light plus shield and a held-item attack remain the existing grab and item
  jump-cancel branches.
- The native combat oracle adds 24 invariants covering both attack buttons,
  exact threshold/neutral/late exclusions, retained momentum, a real 12% hit,
  and mid-action save/load with equal future hashes and events. Browser startup
  repeats every positive and negative route before readiness.
- State schema 27/save format 26 and `PFSAVE26` make the new interpretation fail
  closed while retaining the 522-byte payload and 662-byte checkpoint. Browser
  view schema 23 adds the jump-cancel readiness semantic without changing its
  290-value layout.
- Registry row 27, Jump-cancelling, advances from `planned` to `playable`;
  owner execution and complete cross-target evidence remain before `verified`.

## Delivered in the Pulse Bolt projectile slice

- One original fixed-capacity Pulse Bolt now supplies deterministic
  controller-slot arbitration, grounded and airborne special actions, a
  collision-deferred spawning phase, straight active motion, authored
  lifetime/blast despawn, ordinary hit reaction, shield block, and exact
  two-frame powershield reflection without allocation or dynamic entities.
- `tests/sim/test_m4_projectile.c` adds 38 focused invariants covering content
  validation/hash, simultaneous requests, ground fire/hit, ordinary shield
  block, exact reflection and returned hit, short-hop fire and generic landing,
  save/load future equality, replay verification, and structured/compact RL
  visibility. `tools/verify_m4_projectile.sh` is an independent strict-warning
  verifier check.
- State schema 28/save format 27 and `PFSAVE27` append the 20-byte projectile
  slot, producing a 542-byte payload and 682-byte checkpoint. Input schema 4,
  structured observation schema 4, RL schema 6, and compact observation schema
  5 expose the production special route and six new compact values, for 62
  values total.
- Browser startup must short hop, fire the aerial Pulse Bolt, and land before
  readiness. View schema 24 appends 12 values at indices 290–301, renders the
  cyan projectile and owner, shows its live state, and adds Player 1 `E`, Player
  2 `;`/Numpad 3, and Standard Gamepad top-face controls through the exported
  special-input entry point.
- Registry row 44, Short hop laser, advances from `planned` to `playable`.
  Owner execution and complete native/WebAssembly/browser evidence remain
  before `verified`; projectile powershield reflection also closes the missing
  projectile dependency in row 34.

## Delivered in the Prism Burst reflector slice

- The original data-defined Prism Burst gives down plus fresh special distinct
  grounded and airborne actions. Its one-tick startup leads into a two-tick
  active physical box, three percent damage, authored downward launch, three
  hitlag ticks, and nine recovery ticks; neutral special remains Pulse Bolt.
- While active, that same box reverses a Pulse Bolt's horizontal velocity and
  transfers ownership without applying the powershield result. Ordinary
  two-frame projectile powershield reflection remains unchanged.
- `tests/sim/test_m4_reflector.c` adds 32 invariants covering content
  validation/hash, a grounded downward hit, simultaneous reflector/projectile
  resolution and returned hit, an ordinary-input offstage Shine-spike stock
  route, the unchallenged recovery control, save/load future equality, replay
  verification, and structured/compact RL visibility.
- State schema 29/save format 28 and `PFSAVE28` make the two reflector action
  IDs, hitlag resume, landing, downward-launch, and projectile-reflection
  interpretation fail closed without changing the 542-byte payload or
  682-byte checkpoint. Content schema 30 adds reflector schema 1; inspection
  and browser view schema 25 add the action semantics without changing the
  browser's 302-value layout.
- Browser startup must complete both the offstage hit-to-KO route and an
  unchallenged-victim recovery control before readiness. The live lab enables
  Prism Burst on keyboard and Standard Gamepad input, renders its ordinary
  attack collision box, and names both new actions.
- Registry row 43, Shine spike, advances from `planned` to `playable`.
  Owner execution and complete cross-target evidence remain before `verified`.

## Delivered in the Arc Reservoir charge storage slice

- The original data-defined Arc Reservoir gives grounded full-up plus fresh
  special a distinct charge action. It accumulates to an authored 120-tick
  cap, stores through shield, resumes from the exact stored value, and releases
  a deterministic 4%-to-20% scaled hit through the ordinary combat pipeline.
- Releasing shield before the four-tick store animation ends returns to the
  complete grounded router and preserves charge, including a same-tick normal
  attack. Holding shield through the boundary commits to ordinary shield;
  taking a physical hit during charge or store clears the value.
- `tests/sim/test_m4_charge.c` adds 28 focused invariants covering default and
  invalid data, accumulation/clamp, early store cancel, the held-shield
  negative, exact resume, low/full release damage, interruption loss,
  checksum-valid over-cap load rejection, save/load future equality, replay
  verification, and structured/compact RL visibility.
  `tools/verify_m4_charge.sh` is an independent strict-warning
  verifier check.
- State schema 30/save format 29 and `PFSAVE29` append one `uint16_t` charge
  value per player, producing a 550-byte payload and 690-byte checkpoint.
  Content schema 31 adds charge schema 1, structured observation schema 5 and
  RL schema 7 expose charge, and compact observation schema 6 appends four
  values at indices 62–65 for 66 total.
- Browser startup must charge, enter store, cancel early into an ordinary
  attack, resume, and release before readiness. Inspection and browser view
  schema 26 append charge ticks per player; the browser's event, item, and
  projectile blocks shift by two values for a 304-value view, while controls
  expose up plus special on keyboard and Standard Gamepad.
- Registry row 7, Charge storage canceling, advances from `planned` to
  `playable`. Owner execution and complete cross-target evidence remain before
  `verified`.

## Delivered in the Moonwalk slice

- During `INITIAL DASH`, two ticks of reduced opposite horizontal input enter
  the authored `MOONWALK SETUP`; switching to full opposite input on the next
  tick enters `MOONWALK`, retains the original facing, and applies backward
  initial-dash velocity. Releasing input exits to ordinary grounded traction.
- Immediate full-back reversal and only one reduced-back setup tick both remain
  ordinary dashbacks, so the browser recipe distinguishes correct timing from
  the two nearest mistakes without a hidden per-player history counter.
- `tests/sim/test_m4_movement.c` adds 12 focused invariants for default and
  invalid authored timing, isolated content hashing, exact positive timing,
  facing/dash direction/velocity, traction exit, both negative routes, and a
  690-byte mid-setup save/load with equal future hashes.
- State schema 31/save format 30 and `PFSAVE30` make the two action IDs and
  timing semantics fail closed while retaining the 550-byte payload and
  690-byte checkpoint. Content schema 32/fighter schema 28 hash the authored
  setup duration; inspection and browser view schema 27 retain the 304-value
  layout. Structured observation schema 5, RL schema 7, and compact schema 6
  remain unchanged.
- Browser startup repeats the correct two-tick route plus immediate and
  one-tick dashback controls before readiness, exposes an independent
  `moonwalk_probe`, names both actions, and documents the Shift-to-full input
  transition.
- Registry row 33, Moonwalk, advances from `planned` to `playable`. Owner
  execution and complete cross-target evidence remain before `verified`.

## Delivered in the Teeter-cancel slice

- Releasing horizontal input while grounded residual momentum crosses a
  support edge by no more than the authored 0.4-unit snap distance clamps the
  fighter at that edge in explicit `TEETER`, with zero horizontal velocity and
  a 30-tick neutral duration. Continued outward input still runs off normally,
  and releasing too early stops short.
- Existing standing routers interrupt `TEETER` without a new lockout. The
  focused recipe proves an immediate standing attack and full opposite
  initial dash; jump, shield, crouch, special, grab, and walking remain routed
  by the same production controls.
- `tests/sim/test_m4_movement.c` adds 11 focused invariants for default and
  invalid authored data, isolated content hashing, exact clamp/support/facing,
  neutral duration, both cancels, both negative routes, and a 690-byte
  mid-teeter save/load with equal future hashes.
- State schema 32/save format 31 and `PFSAVE31` make action 73, its tick range,
  grounding, zero-velocity edge state, and cancel semantics fail closed while
  retaining the 550-byte payload and 690-byte checkpoint. Content schema
  33/fighter schema 29 hash the snap distance and duration; inspection and
  browser view schema 28 retain the 304-value layout. Observation and RL
  schemas remain unchanged.
- Browser startup independently performs the attack and reverse-dash cancels,
  held-outward run-off, and early-release control before readiness, exports
  `teeter_cancel_probe`, and names action 73 `TEETER`.
- Registry row 57, Teeter cancel, advances from `planned` to `playable`. Owner
  execution and complete cross-target evidence remain before `verified`.

## Delivered in the Stage-humping slice

- A fresh diagonal-down edge from `GROUND_IDLE` or `CROUCH` enters action 74,
  `CROUCH STEP`, advances exactly the authored 0.1 unit in the chosen
  direction, then settles into ordinary `CROUCH` on the following tick.
  Releasing and repeating composes the researched Stage-humping motion without
  adding a technique-only input.
- Existing canonical down-edge history gates repetition: holding the diagonal
  produces only one step. Neutral down remains stationary crouch, horizontal
  alone remains dash, and down on pass-through support retains platform drop.
- `tests/sim/test_m4_movement.c` adds ten focused invariants covering default
  and invalid authored data, isolated hashing, exact bidirectional movement,
  eight release/reset repetitions, all three negative controls, and a
  690-byte mid-step save/load with equal future hashes.
- State schema 33/save format 32 and `PFSAVE32` make the action ID, tick range,
  grounding, and one-step transition fail closed while retaining the 550-byte
  payload and 690-byte checkpoint. Content schema 34/fighter schema 30 hash
  the speed and duration; inspection/browser view schema 29 retain the
  304-value layout. Observation and RL schemas remain unchanged.
- Browser startup independently repeats the release-gated route and held,
  neutral-down, and horizontal-only controls, exports `stage_humping_probe`,
  and names action 74 `CROUCH STEP`.
- Registry row 50, Stage humping, advances from `planned` to `playable`. Owner
  execution and complete cross-target evidence remain before `verified`.

## Delivered in the projectile-camping slice

- The existing one-slot Pulse Bolt, grounded fire recovery, ordinary run/jab
  input, stage positions, and canonical tick clock now compose a bounded
  projectile-camping route without adding mutable state or a technique-only
  action.
- The 180-tick positive policy fires only after the prior bolt resolves. Seven
  legal fires produce six hits, keep at least 693,712 Q16.16 units (10.58
  world units) of center separation, and leave the camper at 0% while the
  responder continuously approaches and requests jabs.
- A Reset control preserves the content and responder policy but omits every
  projectile; the opponent closes the gap and lands three physical hits. Both
  traces reject termination/truncation, making the bounded clock and failure
  case explicit.
- `tests/sim/test_m4_projectile.c` expands to 46 invariants and prints the
  exact trace counts. Browser startup independently repeats both routes and
  exports `camping_probe` before restoring default content.
- Registry row 5, Camping, advances from `planned` to `playable`. Owner
  execution, broader stages/projectiles, and complete cross-target evidence
  remain before `verified`.

## Classified in the emergent-turtling slice

- Turtling adds no new mechanic or dedicated oracle. Its researched definition
  is avoiding the opponent, using ranged attacks, and punishing bad approaches;
  the existing projectile-camping trace already performs all three against a
  continuous approach-and-jab responder.
- Seven legal fires produce six ranged hits while maintaining more than 10.58
  units of separation and taking zero damage. The existing no-projectile
  control lets the identical responder land three jabs, while the independent
  Approach/Spacing route proves the safe whiff punish.
- Registry row 58 advances from `primitive-ready` to `playable` using those
  independently tested production mechanics. Broader stage/fighter policies,
  complete cross-target/replay evidence, and owner execution remain before
  `verified`.

## Classified in the emergent-stalling slice

- Stalling adds no new mechanic or dedicated oracle. The existing playable
  Planking route already repeats legal conflict-avoidance cycles against an
  attacking opponent through the canonical ledge, air-jump, and timer state.
- Its three exact-boundary drop/double-jump/regrab cycles demonstrate the
  time-extending route and resource/protection refresh; the existing two-tick
  fast-fall control demonstrates the vulnerability limit and responder punish.
- Registry row 52, Stalling, advances from `planned` to `playable` using that
  production-path native/browser evidence. Owner execution and broader
  avoidance policies remain before `verified`.

## Classified in the emergent-infinite slice

- Infinite adds no new mechanic or dedicated oracle. The existing fast-jab
  fixture can repeat its earliest-idle jab cycle against a wall with near-zero
  knockback growth; canonical damage caps at 999%, so the pinned cycle has no
  exhaustible damage resource after saturation.
- Existing zero-to-death evidence already proves a 21-jab uninterrupted
  prefix, mid-chain save/load equality, native/browser execution, and an
  open-stage outward-DI branch that makes the later active jab whiff.
- Registry row 20, Infinite, advances from `planned` to `playable`. A longer
  repeated-state trace and owner execution remain before `verified`.

## Implemented in the Taunt-cancel slice

- Input schema 5 assigns a dedicated bit-4 Taunt control. The original fighter
  authors a grounded `TAUNT` with a 61-count terminal counter; entry retains
  dash velocity, ordinary
  traction decelerates it, action routers remain locked, and a held button
  cannot retrigger without release.
- Releasing horizontal input and pressing Taunt just before retained momentum
  crosses the facing support edge lets the existing edge transition replace
  `TAUNT` with `TEETER`. Starting near center stage instead proves the complete
  90-tick recovery and exact return to idle.
- State schema 34/save format 33 and `PFSAVE33` retain the 550-byte payload and
  690-byte checkpoint while failing closed on the new action and tick range.
  Content schema 35/fighter schema 31 hash the authored duration; inspection
  and browser view schema 30 version the same 304-value interpretation.
- The focused native movement oracle reaches 285 invariants, including
  invalid/default content, isolated hashing, retained momentum, exact recovery,
  input lock, held non-repetition, edge cancellation, and mid-action save/load
  future equality. Browser startup repeats both outcomes and exposes
  `taunt_cancel_probe`; live keyboard/gamepad routes use `T`/`,` or Back/View.
- Registry row 53, Taunt cancelling, advances from `planned` to `playable`.
  Owner execution and broader fighter/stage authored variations remain before
  `verified`.

## Implemented in the Scar-Jump slice

- The original fighter now authors a normal wall jump at 0.3 horizontal and
  -0.5 vertical unit per tick, with a 24-tick `WALL_JUMP` action and four
  initial invulnerability ticks.
- An ordinary jump from the right ledge reaches the raised block wall. A fresh
  full-away input at contact starts the wall jump without consuming the saved
  air jump; either an aerial or that saved jump can cancel the action. An
  early-away route misses the wall and remains the negative control.
- State schema 35/save format 34 and `PFSAVE34` retain the 550-byte payload and
  690-byte checkpoint while failing closed on the new action and tick range.
  Content schema 36/fighter schema 32 hash the authored speeds, duration,
  invulnerability, and enable flag. Inspection and browser view schema 31
  version the unchanged 304-value layout.
- The focused native movement oracle reaches 297 invariants, including the
  production geometry, exact launch, preserved air jump, action lock and
  cancels, negative timing control, and mid-action save/load future equality.
  Browser startup repeats the route and exports `scar_jump_probe`.
- Registry row 38, Scar Jump, advances from `planned` to `playable`. Owner
  execution and broader fighter/stage authored variations remain before
  `verified`.

## Implemented in the Team-Wobble slice

- No new combat action or canonical state was added. Four-player team mode,
  reciprocal grab links, same-team rejection, the existing low down throw,
  hitlag, grab recovery, and ordinary fresh light-plus-shield input compose the
  alternating handoff.
- A narrow 0.4-unit-spacing team lab assigns allied P1 and P3 to the two
  physical controller slots, gives captured P2 ordinary alternating mash
  edges, and leaves P4 neutral. The live browser button switches between this
  fixture and the default item duel without presentation-owned simulation.
- The focused combat oracle reaches 596 invariants and performs two typed
  down-throw/fresh-grab handoffs with reciprocal owner/target links and exact
  accumulated damage. Its early-waiting-grab control spends the active window
  before release and lets the victim escape.
- Canonical state schema 35, save format 34, content schema 36, input schema 5,
  and inspection schema 31 remain unchanged. Browser view schema 32 expands
  the view from 304 to 392 values by exposing the existing P3/P4 inspection
  records before the shifted event, item, and projectile blocks.
- Browser startup exports `team_wobble_probe`; the live adapter renders four
  fighter cards, maps the second controller to simulation slot 2, and exposes
  the handoff recipe. Registry row 54 advances from `planned` to `playable`;
  owner execution and complete cross-target/replay evidence remain before
  `verified`.

## Implemented in the Vector-Ascent recovery slice

- The original recovery definition authors a 1/4-unit horizontal speed,
  4/5-unit upward launch, and 18-tick `VECTOR_ASCENT`. Full-up plus fresh
  Special selects it only while airborne and ready; the same grounded input
  remains Arc Reservoir.
- Entry spends one canonical recovery byte, clears fast fall/tumble, steers
  under ordinary gravity, and finishes in `FALL_SPECIAL`. A second attempt is
  ignored until landing, ledge grab, stock loss/respawn, or reset restores the
  byte; interruption does not refund it.
- State schema 36/save format 35 and `PFSAVE35` append four bytes for a
  554-byte payload and 694-byte checkpoint. Content schema 37/recovery schema
  1 hash the authored definition. Inspection schema 32 and observation schema
  6 expose availability; RL schema 8/compact schema 7 pack it into player flag
  bit 18 while keeping 66 values.
- Nine focused movement invariants cover data validation/hash identity, entry,
  velocity, consumption, mid-action save/load future equality, blocked reuse,
  landing restoration, second-airtime reuse, and structured/compact RL
  visibility. Browser readiness exports `vector_ascent_probe`; view schema 33
  appends four READY/SPENT flags at indices 392–395 for 396 values.
- Registry rows 19 (Gimp) and 51 (Stage spike) advance from `primitive-ready`
  to `playable` by composing this independently checked recovery with existing
  edgeguard hit/KO and surface-bounce/tech/stock mechanics. In accordance with
  the emergent-technique policy, no duplicate tactic-only harness is added.

## Implemented in the grab-pummel slice

- A fresh neutral or reduced-stick light/strong attack during `GRAB_HOLD`
  enters explicit `PUMMEL`; full-direction fresh attacks continue to select
  the four authored throws.
- The original fighter authors 3% non-launching damage at tick 2 of a ten-tick
  action. One typed `PUMMEL` event records damage and attribution with zero
  launch, while reciprocal links, tethering, and the victim's ordinary mash
  countdown remain active. The holder returns to `GRAB_HOLD`; holding attack
  through recovery cannot retrigger without a fresh edge.
- State schema 37/save format 36 and `PFSAVE36` retain the 554-byte payload and
  694-byte checkpoint while failing closed on the new action, timing, live-link,
  and event semantics. Content schema 38/fighter schema 33 hash damage, hit
  tick, and duration. Inspection schema 33 and browser view schema 34 version
  the unchanged inspection and 396-value presentation layouts.
- The focused combat oracle reaches 620 invariants, including invalid/default
  data, isolated hashing, both attack buttons, reduced-stick entry, exact
  event/damage/link behavior, held-input rejection, and mid-pummel save/load
  future equality. Browser startup repeats the complete pummel before all four
  throws and the low-percent chain route. This primitive is not a 61-row
  emergent-technique entry, so registry status does not change.

## Implemented in the crouch-cancel slice

- A defender already in grounded `CROUCH` qualifies an eligible physical hit
  only when its post-hit damage is at or below the authored inclusive 40%
  ceiling. Standing/released-down, airborne, over-ceiling, throw, armor, reset,
  and shield routes retain ordinary reaction semantics.
- Damage, attribution, and hitlag remain unchanged. Both pending launch
  components and hitstun use independent authored 2/3 Q16.16 scales, with a
  one-tick floor for nonzero hitstun; tumble is derived afterward. The typed hit
  event carries the scaled vector and `PF_SIM_EVENT_FLAG_CROUCH_CANCEL`.
- State schema 38/save format 37 and `PFSAVE37` retain the 554-byte payload and
  694-byte checkpoint while failing closed on the new reaction semantics.
  Content schema 39/fighter schema 34 hash the ceiling and both scales;
  inspection schema 34 and browser view schema 35 version the unchanged
  inspection and 396-value presentation layouts.
- The combat oracle reaches 650 invariants with standing/crouched/released-down equivalence,
  exact scaling, invalid/hash-sensitive data, inclusive/first-over boundaries,
  typed-event/tumble consistency, and mid-hitlag save/load future equality.
  Browser readiness folds the same comparison into the existing reaction probe
  and the live/replay feeds label the flag `CROUCH CANCEL`.

## Implemented in the victim-weight slice

- The original fighter now authors Q16.16 victim weight, 1.0 by default with
  an inclusive validated precursor range of 0.5–2.0. The shared unblocked
  reaction path divides both post-damage launch components by target weight,
  so fighter, item, projectile, reflector, charge-release, and throw hits cannot
  drift into source-specific formulas.
- Hitstun and delayed-air-jump armor qualification use the weighted vector;
  crouch/V-cancel and DI retain their established later ordering. Damage,
  attribution, hitlag, shield blocks, and shield-break launch are unchanged.
- Content schema 40/fighter schema 35 hash the field. State schema 38/save
  format 37, the 694-byte checkpoint, replay, inspection 34, browser view 35,
  observation 6, RL 8, compact 7, and 66 compact values remain unchanged.
- The combat oracle reaches 666 invariants, covering default and both accepted
  boundaries, first-invalid bounds, hash identity, exact two-axis halving for a
  2.0-weight defender, hitstun recomputation, and unchanged damage/hitlag. The
  existing browser reaction probe repeats the non-default comparison in Wasm
  and restores default content before readiness.

## Implemented in the directional-ground-attack slice

- A fresh grounded light edge with full-threshold, strictly vertical-dominant
  input now selects explicit `UP_ATTACK` or `DOWN_ATTACK`. Reduced vertical
  input remains the neutral jab, while full horizontal and equal diagonals
  retain forward-smash priority. Direct strong and run light retain their
  existing strong and dash-attack routes.
- Two embedded fighter attack records independently author and hash box
  geometry, damage, horizontal/vertical base knockback, growth, startup,
  active, recovery, and hitlag. The shared combat path signs horizontal launch
  by facing and vertical launch by action, then applies ordinary shield,
  weight, crouch/V-cancel, DI/SDI, event, and once-per-target semantics.
- State schema 39/save format 38 and `PFSAVE38` retain the 554-byte payload and
  694-byte checkpoint while failing closed on action IDs 79/80, arbitration,
  timing, hitlag resume, and powershield routing. Content schema 41/fighter
  schema 36, inspection schema 35, and browser view schema 36 version the new
  data and action labels without changing observation or presentation counts.
- The focused combat oracle reaches 708 invariants, covering defaults,
  invalid/hash-sensitive data, exact up/down selection and launch, neutral and
  equal-diagonal controls, direct-strong priority, typed events, hitlag, and
  mid-hitlag save/load future equality. Existing powershield-cancel coverage
  exercises neutral/up/down light and strong selection; browser readiness folds
  the directional cases into its ordinary attack and shield probes. No
  emergent-technique-only harness was added.

## Implemented in the directional-aerial slice

- A fresh airborne light edge now selects the complete five-direction normal
  vocabulary. Neutral or reduced input remains `AERIAL_ATTACK`; a full,
  strictly vertical-dominant stick selects `UP_AERIAL` or `DOWN_AERIAL`; and a
  full horizontal-dominant or equal-diagonal stick selects `FORWARD_AERIAL` or
  `BACK_AERIAL` relative to facing. The direct strong button remains
  `STRONG_AERIAL_ATTACK`. The same selector applies from ordinary air,
  delayed-air-jump cancel, and wall-jump cancel routes.
- Four embedded fighter attack records independently author and hash box
  geometry, damage, signed two-axis base knockback, growth, startup, active,
  recovery, and hitlag. All five light aerials use the existing shared
  physical-hit, shield, weighted-reaction, DI/SDI, journal, once-per-target,
  auto-cancel, 12-tick landing-lag, and six-tick L-cancel paths.
- State schema 40/save format 39 and `PFSAVE39` retain the 554-byte payload and
  694-byte checkpoint while failing closed on action IDs 81–84, grounding,
  timing, and hitlag resume. Content schema 42/fighter schema 37, inspection
  schema 36, and browser view schema 37 version the new data and labels while
  retaining the 396-value browser view, input schema 5, observation schema 6,
  RL schema 8, compact schema 7, and 66 compact values. The opaque simulation
  storage requirement grows from 1,920 to 2,080 bytes because authored content
  is copied into the simulation; existing callers use the 4 KiB M4 envelope,
  while canonical checkpoint size remains unchanged.
- The focused combat oracle reaches 780 invariants and covers all four new
  defaults, invalid and hash-sensitive data, exact input arbitration, signed
  launch, damage, hitstun, hitlag, typed event identity, and mid-hitlag
  save/load future equality. Browser readiness folds neutral, four-direction,
  and direct-strong arbitration into the existing combat probe. Tactical and
  emergent rows continue to reuse constituent mechanics; no emergent-only
  harness was added.

## Implemented in the ledge-option slice

- After the seven-tick catch lock, fresh light or strong attack now selects
  canonical `LEDGE_ATTACK`; a fresh trigger without attack selects canonical
  `LEDGE_ROLL`. Attack wins the simultaneous light-plus-trigger chord. Inputs
  held from catch lock do not repeat, while jump, down/away release, and inward
  climb retain their existing routes.
- The original fighter authors a 7/4-unit inward ledge roll with 20 movement
  ticks, 30 total ticks, and action-derived invulnerability through tick 21.
  Its ledge attack has six startup, three active, and 20 recovery ticks, deals
  10%, applies five hitlag ticks through shared combat, and remains
  action-invulnerable through tick 9. Both actions retain the ledge claim and
  finish through the existing floor-landing path.
- State schema 41/save format 40 and `PFSAVE40` retain the 554-byte payload and
  694-byte checkpoint while failing closed on action IDs 85/86, timing,
  grounding, ledge claims, invulnerability, and attack hitlag resume. Content
  schema 43/fighter schema 38, inspection schema 37, and browser view schema 38
  version the authored data and labels while retaining the 396-value browser
  view, input schema 5, observation schema 6, RL schema 8, compact schema 7,
  and 66 compact values. The opaque simulation storage is now 2,128 bytes;
  scratch storage remains 1,008 bytes and the 4 KiB caller envelopes remain
  valid.
- The movement oracle reaches 334 invariants and covers held-trigger rejection,
  exact roll interpolation, invulnerability expiry, completion, content
  validation/hash sensitivity, and mid-roll future hashes. The combat oracle
  reaches 815 invariants plus 51 journal invariants and covers held-attack
  rejection, attack-over-trigger priority, both attack buttons, concurrent-jab
  rejection, typed damage/attribution, retained ledge ownership through
  attacker hitlag, and mid-hitlag future hashes. Browser
  readiness folds all three option inputs and the active hitbox into the
  existing combat probe. No emergent-technique-only harness was added.

## Implemented in the grounded-normal and smash-charge slice

- Reduced directional light now completes the grounded tilt vocabulary with
  `FORWARD_ATTACK` alongside the existing up/down actions. The direct strong
  button selects immediate neutral/forward/up/down strong attacks. Full
  directional light from idle or walk enters the matching smash charge; the
  established one-through-three-tick initial-dash window enters forward charge
  with retained movement, while frame 4 remains the forward tilt.
- Holding light advances an independent canonical timer through tick 60.
  Releasing early or reaching tick 60 starts the matching directional strong;
  the default Q16.16 bonus scales damage linearly to +50% at maximum. The timer
  survives save/load, release, and attacker hitlag, then clears on completion,
  stock loss, or interruption. Direct strong, powershield cancel, and
  jump-squat cancel routes remain immediate and uncharged.
- State schema 42/save format 41 first retained the 554-byte payload and
  694-byte checkpoint under `PFSAVE41` while adding action IDs 87–90 and four
  authored attack records. State schema 43/save format 42 then appends four
  charge counters for a 562-byte payload and 702-byte checkpoint under
  `PFSAVE42`; charge action IDs 91–93 and every timer/action relationship fail
  closed. Content schema 45/fighter schema 40, inspection schema 39,
  observation schema 7, RL schema 9/transition schema 7, compact schema 8 with
  70 values, and browser view schema 40 with 400 values expose the final
  contract. Opaque requirements are 2,312 state bytes and 1,016 scratch bytes,
  within the existing 4 KiB envelopes.
- The existing 815-mechanic/51-journal combat oracle now covers authored
  forward/up/down light and strong data, selection, shields, hitlag, partial and
  full charge damage, slot-independent simultaneous charged trades, mid-charge
  save/load, 60-tick automatic release, and interruption clearing. Browser
  readiness reuses its ordinary directional
  attack probe; no tactical or emergent-only harness was added.

## Implemented in the analog light-shield slice

- The stronger raw trigger value now enters shield at the authored 8,192 light
  threshold; input below it remains actionable, 8,192–32,767 is light, and the
  established 32,768 digital threshold begins dense shield. Keyboard keys and
  gamepad bumpers remain full-density inputs, while Standard Gamepad analog
  triggers preserve their browser-reported pressure.
- Light hold depletion interpolates from the authored 0.07 HP per tick to the
  dense 0.28 HP per tick. Ordinary block damage and shield stun are unchanged;
  defender pushback uses an additional authored multiplier that interpolates
  from 1.25 at the light threshold to 1 at dense. Light shield cannot
  powershield physical attacks or projectiles. Roll, spot dodge, grab, platform
  drop, release, and jump-cancel routes continue through the shared shield
  primitive.
- A block freezes the qualifying collision strength through hitlag and shield
  stun, then adopts current held pressure when ordinary shield resumes. This
  keeps dense powershield provenance load-valid while permitting subsequent
  analog adjustment.
- State schema 44/save format 43 appends four raw strength values for a 570-byte
  payload and 710-byte checkpoint under `PFSAVE43`, with fail-closed action,
  threshold, powershield, hitlag-resume, and inactive-slot relationships.
  Content schema 46/fighter schema 41, inspection schema 40, observation schema
  8, RL schema 10/transition schema 8, compact schema 9 with 74 values, and
  browser view schema 41 with 404 values expose the contract. Opaque requirements
  are 2,328 state bytes and 1,024 scratch bytes, within the existing 4 KiB
  envelopes.
- The combat oracle reaches 829 mechanics invariants and covers entry
  boundaries, exact/midpoint/dense depletion, inspection/observation, save/load,
  deterministic future hashes, light-versus-dense block pushback, and
  dense-only powershielding. RL and browser adapters independently exercise the
  primitive. Tactical rows continue to reuse constituent primitive evidence;
  no emergent-technique-only harness was added.

## Implemented in the shield-geometry, tilt, and poke slice

- Shield collision now uses a distinct deterministic AABB derived from
  authored 0.8-by-1.4 half extents. The scale is
  `0.15 + 0.85 * health_fraction * density`; density is 1 at the light
  threshold, interpolates downward with trigger pressure, and is 0.5 at the
  dense threshold. Light shields are therefore larger at equal health and all
  shields shrink as health is lost.
- Canonical signed x/y main-stick tilt moves the shield center up to an
  authored 0.3 unit per axis after the ordinary dead zone. Tilt samples
  directly while shielding, freezes with collision strength through shield
  hitlag/stun, survives save/load and rollback, and clears everywhere the
  shield lifecycle ends.
- Physical attacks, thrown items, and projectiles block wherever their hitbox
  overlaps shield, including outside the body. Shield wins where shield and
  hurtbox both overlap. A hitbox that reaches only exposed hurtbox takes the
  ordinary hit path and clears shield state; grabs remain hurtbox-only.
- State schema 45/save format 44 adds four signed x-tilt and four signed y-tilt
  values for a 586-byte payload and 726-byte checkpoint under `PFSAVE44`.
  Content schema 47/fighter schema 42, inspection schema 41, observation schema
  9, RL schema 11/transition schema 9, compact schema 10 with 86 values, and
  browser view schema 42 with 431 values expose the contract. Opaque
  requirements are 2,368 state bytes and 1,040 scratch bytes within the
  unchanged 4 KiB envelopes.
- The existing combat executable now covers 884 mechanics invariants, including
  exact formula/bounds, light-versus-dense size, signed tilt, observation/RL
  exposure, save/load continuation, centered block priority, and an identical
  attack becoming a poke only when tilt exposes the hurtbox. The browser
  collision inspector draws the exact shield AABB and retains the 61-row owner
  checklist; no emergent-technique-only harness was added.

## Implemented in the shield-SDI and shield-ASDI slice

- Grounded hitlag that resumes into `SHIELD_STUN` now accepts horizontal
  component-edge SDI at an authored scale of ordinary target SDI, exactly
  33/50 by default. A held horizontal component does not repeat, vertical
  additions and vertical-only input are ignored, and opposite-direction
  re-entry produces a new pulse.
- The final shield-hitlag input applies one horizontal-only ASDI displacement
  at the same authored scale before shield stun begins. Shield reaction never
  applies trajectory DI, and every shift uses the existing collision-safe
  hitlag-displacement path while clamping to the current support edge.
- Content schema 48/fighter schema 43 append, validate, and hash the scale.
  State schema 45/save format 44, inspection/observation/RL/browser layouts,
  the 726-byte checkpoint, 2,368-byte state, and 1,040-byte scratch requirement
  remain unchanged.
- The existing combat executable now covers 921 mechanics invariants, including
  exact horizontal pulse/ASDI displacement, held/vertical negative routes,
  opposite-direction re-entry, support-edge clamping, content
  validation/identity, and post-pulse mid-shield-hitlag save/load continuation.
  The existing browser shield startup probe exercises the same production
  route; no emergent-technique-only harness was added.

## Implemented in the moving revival-platform slice

- Stocked fighters now leave the inactive respawn wait on a per-player moving
  platform at the deterministic centered slot x. The default authored platform
  descends from y=4 to y=12 over 30 ticks, holds for 90 ticks, and has a
  two-unit half-width.
- The fighter is pinned to the platform with zero velocity, recovery available,
  and collision invulnerability. Gameplay input is ignored during descent.
  After the endpoint, any ordinary stick/button/analog-shield input releases
  into `AIRBORNE`; neutral input releases at timeout. Only release starts the
  configured post-drop respawn-invulnerability timer.
- `REVIVAL_DROP` is a typed system-source event with detail 0 for player input
  or 1 for automatic timeout. Inspection and the append-only browser tail expose
  exact active left/right/y geometry, and the live page renders the platform,
  drop prompt, state-card status, and event-feed reason.
- Content schema 49/stage schema 3 author and hash platform geometry/timing.
  State schema 46/save format 45 retains the 586-byte payload and 726-byte
  checkpoint under `PFSAVE45` while making action 94/support 4 and exact
  derived-position lifecycle rules fail closed. Inspection schema 42 and
  browser view schema 43 with 447 values expose the slice; fighter,
  observation, and RL/compact layouts are unchanged. The immutable stage copy
  raises opaque state storage from 2,368 to 2,384 bytes; scratch remains 1,040
  bytes and both stay inside the existing 4 KiB envelopes.
- The existing match suite adds 24 revival invariants, including invalid/hash
  content cases, exact interpolation, ignored early input, a mid-platform
  save/load continuation, input/automatic release events, and post-drop
  invulnerability. The existing browser match probe covers the ordinary input
  path; no emergent-technique-only harness was added.
- Exact commit `fecd6ac9c03f05145fc00dd2ed873dad602c3d60` passes Windows
  MSVC Release 22/22, WSL GCC 13.3 Release 22/22, WSL ASan/UBSan 22/22, the
  strict M2/M4 source gates, pinned Emscripten 6.0.3, native/WebAssembly replay
  equality, and the live Microsoft Edge DOM/Wasm smoke. Two repeated verifier
  soaks produce the same `97cb2b4ca946b2ed` digest.
- Two clean unsampled Windows milestone benchmark runs classify all 10
  measurable scenarios as compatible, with zero suspected or confirmed
  regressions. The benchmark database confirms 2,384 public state bytes and a
  726-byte checkpoint. A separate Tracy 0.13.1 profile-only capture passes in
  WSL with the supported timer fallback enabled; its 11,791-byte trace has
  SHA-256
  `79c40233d75676750bba213579d6cb679dbb347c414b7ace8805af3565d4aa74`.

## Implemented in the stale-move slice

- Every player now owns a fixed nine-entry newest-first queue of canonical move
  IDs. Damage is multiplied by one minus the authored reductions in matching
  slots before the current hit registers. The exact default reductions total
  at most 35.15625%, use deterministic integer flooring, and are validated,
  hashed, and isolated per owner.
- Only successful hurtbox hits register. Whiffs add nothing; shield contacts
  use the already-staled damage for shield damage, stun, and pushback without
  inserting. Physical attacks and pummels/throws use per-action deduplication,
  thrown items use a per-flight latch, and the single projectile registers to
  its current owner, including after reflection. Ground/air item, projectile,
  and reflector variants share their intended canonical identity.
- Queues survive ordinary stock loss, respawn wait, and revival-platform drop.
  New-match and sudden-death player rebuilds clear them. Save/load, clone,
  rollback, and replay preserve the queue and live registration latches.
  Inspection, structured observation, compact RL, and browser state cards
  expose their appropriate queue/count/multiplier views.
- Content schema 50/fighter schema 44 define the authored weights. State schema
  47/save format 46 emits a 631-byte payload and 771-byte checkpoint under
  `PFSAVE46`; inspection schema 43, observation schema 10, RL schema 12/
  transition schema 10, compact schema 11 with 102 values, and browser schema
  44 with 496 values expose the contract. Opaque requirements are 2,448 state
  bytes and 1,080 scratch bytes within the unchanged 4 KiB envelopes.
- The existing combat executable now covers 958 mechanics invariants. Its 37
  new assertions cover all authored weights, repeated-move floors and queue
  order, owner isolation, different-move shifting, hurt/shield/whiff/reset
  boundaries, content identity/validation, observation/RL parity, and canonical
  pummel, throw, item, projectile, reflected-owner, reflector, and charge IDs.
  Existing tactical rows reuse the changed primitive behavior; no
  emergent-technique-only harness was added.
- Exact clean commit `8eed68f4333248ae9a8abf584070bc0ca1ac653c`
  passes Windows MSVC Release 22/22, WSL GCC 13.3 Release 22/22, pinned
  Emscripten 6.0.3, and the live Microsoft Edge DOM/Wasm smoke. Native Windows,
  WSL, and WebAssembly emit the same 31,463-byte replay and all three published
  digests. Windows and WSL verifier self-tests repeat the same
  `34648cdeb4e2d4e1` match-soak digest.
- Its clean unsampled Windows milestone benchmark classifies all 10 measurable
  scenarios as compatible, with zero invalid comparisons and zero suspected or
  confirmed regressions. Relative to the exact pre-stale `fecd6ac` raw medians
  in the same qualified database, 1v1 and 2v2 are +0.58% and +0.28%, save and
  restore are +32.88% and +25.12%, rollback is +12.72%, and replay verification
  is +26.72%; empty tick, maximum entities, single RL, and batched RL are
  -5.21%, -1.32%, -1.63%, and -0.03%. These cross-revision percentages are
  descriptive; the compatible classifier result is the acceptance decision.
- A clean Tracy 0.13.1 profile-only capture passes in WSL with the supported
  timer fallback enabled. Its 11,533-byte local trace has SHA-256
  `04c6e3c6f475abc21bad130b2789da98fae0d8a66c8fb5d065ee7b3b9adfb14b`;
  the manifest records `perf` as unavailable because it is not installed.

## Implemented in the stationary upper-platform slice

- The default stage now adds a bounded stationary one-way deck from x = 16 to
  x = 24 at y = 13 above the existing raised block. It has canonical support
  ID 5. The moving center platform remains the only surface that carries a
  grounded fighter horizontally.
- Descending collision selects the first crossed eligible surface among the
  solid top, moving platform, upper platform, and floor. Both one-way surfaces
  share ordinary down drop-through, reduced-down shield drop, grounded support
  bounds/teeter behavior, hitlag correction, and nearest eligible drop-cancel
  handling. Upward motion continues through either deck.
- Content validation rejects empty/out-of-floor bounds, invalid vertical
  placement, same-height moving-platform overlap, revival-descent overlap, and
  solid-block overlap. Center/y/half-width participate in the content hash.
  Inspection and the browser collision overlay expose exact left/right/y
  geometry; the default playtest renders the upper deck in pale pink.
- Content schema 51/stage schema 4 and inspection schema 44 expose the new
  immutable geometry. State schema 48/save format 47 retains the 631-byte
  payload and 771-byte checkpoint under `PFSAVE47`; browser schema 45 appends
  indices 496–498 for 499 values. Observation/RL layouts remain unchanged.
  Opaque requirements are 2,464 state bytes and 1,080 scratch bytes within the
  existing 4 KiB envelopes.
- The movement executable now covers 349 mechanics invariants, including the
  deck's authored defaults and invalid cases, landing, fixed support, exact
  inspection, ordinary and shield drop-through, and equal save/load future
  hashes. Existing non-geometry browser probes use explicit compact-stage
  fixtures where necessary; no emergent-technique-only harness was added.
- Exact clean commit `39bb0c4590b8d000f73b69996ecbde14f2b62892`
  passes Windows MSVC Release 22/22, WSL GCC 13.3 Release 22/22, pinned
  Emscripten 6.0.3, and the live Microsoft Edge DOM/Wasm smoke. Native Windows,
  WSL, and WebAssembly emit the same 31,463-byte replay and all three published
  digests. Windows and WSL verifier self-tests repeat the same
  `7723e97268932d86` match-soak digest.
- A qualified native-Windows comparison against exact clean pre-platform commit
  `8eed68f4333248ae9a8abf584070bc0ca1ac653c` classifies every one of the 10
  measurable scenarios as compatible. A second same-commit run repeats that
  result; both current runs record zero invalid comparisons and zero suspected
  or confirmed regressions. The largest raw median changes against the baseline
  are +1.97% for empty tick and +1.70% for replay verification; all other
  changes lie between -1.01% and +0.31%.
- A clean Tracy 0.13.1 profile-only capture passes in WSL with the supported
  timer fallback enabled. Its 11,776-byte local trace has SHA-256
  `e7d561777aa10bce2d84c4288f293bb2367bb0828b9d9a669851818dce9d6be8`;
  the manifest records `perf` as unavailable because it is not installed.

## Implemented in the prone-orientation getup-roll slice

- Missed-tech landing now records `BACK` or `STOMACH` from incoming horizontal
  motion relative to facing. The orientation survives knockdown, down wait,
  and floor recovery, then clears when ordinary grounded state resumes.
- Fighter-authored, hash-participating getup-roll schedules now distinguish all
  four orientation/relative-direction routes. Back/forward begins movement on
  frame 6 and is invulnerable on frames 1–19; back/backward uses frame 12 and
  frames 12–29; stomach/forward uses frame 8 and frames 1–19; and
  stomach/backward uses frame 5 and frames 1–24. Every route lasts 35 ticks.
- Content schema 52/fighter schema 45, inspection schema 45, state schema 49,
  observation schema 11, RL schema 13/transition schema 11/compact schema 12,
  and browser schema 46 make the interpretation fail closed. The 102-value RL
  vector reuses player-flags bits 19–20; browser values 499–502 produce a
  503-value view and readable `prone none/back/stomach` state cards.
- Save format 48 keeps the stream at 771 bytes by packing direction and
  orientation into one byte per fighter. Canonical decoding rejects reserved
  direction/orientation codes and high bits even under a recomputed checksum.
  Byte-sized timing records keep opaque requirements to 2,472 state bytes and
  1,088 scratch bytes. This removes the provisional 775-byte stream's extra
  SHA-256 compression block and its measured checkpoint-path regressions.
- Exact clean commit `91d69f5f03a3a6205011af61f99c3d23c6d88f6d`
  passes Windows MSVC Release 22/22, WSL GCC 13.3 Release 22/22, strict
  `-Wconversion -Werror` combat/kernel checks, pinned Emscripten 6.0.3, and a
  byte-identical native/WebAssembly replay comparison. The live generated-Wasm
  page reports `playtest=ready`, replay and floor-recovery probes passing,
  exactly 61 owner recipes, and readable prone-orientation cards. The combat
  oracle now covers 982 mechanics invariants plus 51 journal invariants,
  including all four schedules, observation exposure, content validation, and
  mid-roll continuation.
- The 31,463-byte replay has corpus SHA-256
  `3c7130d92683b83e6b6260e74907c6719f0e511d043fdbb1185e1d70403b50e1`,
  final-state SHA-256
  `6fa0766f63c3582bfd61edbd231ee455c59ae4ca2729e80b3d10b0cd981ea405`,
  and unchanged event digest
  `7dac547f463ec6995207dc41d8fab3449113b79cd6179d4037e821a8dc63b18f`.
  Windows and WSL verifier self-tests repeat match-soak digest
  `aa70215a3998a1f3`.
- Three clean native-Windows milestone runs compare exact pre-orientation
  commit `5ba2eadb9250eeb340751691b06103fee00ed6f6` with the final commit and
  repeat the final commit. All 10 measurable scenarios are compatible in both
  comparisons, with zero invalid, suspected, or confirmed regressions. A clean
  WSL Tracy 0.13.1 capture passes with timer fallback enabled; its 11,814-byte
  trace has SHA-256
  `5a720f86d7ece6c609d71a7864322a418456c60c544f5af2dc289cbcc6cebe4f`,
  while `perf` remains unavailable because it is not installed.

## Implemented in the complete action-transition journal slice

- ABI-4 event type 24 now journals every final current-player action change in
  one system record. `value_q16` packs the four final action bytes,
  `velocity_x_q16` packs the four tick-start action bytes, and `detail` is the
  nonzero changed-player mask. Multiple changes within one tick collapse to
  the tick boundary, and returning to the starting action emits no false row.
- Simultaneous forfeits now coalesce into one system event whose detail is the
  forfeiting-player mask, followed by one match-result event. The statically
  proven production maximum is 14 records against the unchanged capacity of
  16; action transitions follow movement/item/combat/projectile resolution and
  match resolution remains last.
- State schema 50/save format 49, inspection schema 46, and browser schema 47
  make the new event meanings fail closed while preserving the 631-byte
  payload, 771-byte checkpoint, 503-value browser view, 2,472-byte opaque
  state requirement, and 1,088-byte scratch requirement. Content, fighter,
  observation, RL, transition, and compact schemas remain unchanged.
- The hot path maintains a scratch-only changed-player mask while existing
  movement/item/combat code writes actions, so an unchanged tick performs no
  separate action-array scan and emits no event. This recovered the measured
  action-journal regression without changing canonical or replay bytes beyond
  the intended schema/event interpretation.
- The combat oracle remains at 982 mechanics invariants and rises to 74
  journal invariants. The match oracle retains 24 mechanics plus 24 revival
  invariants and rises to 62 journal invariants, including exact simultaneous
  transitions, neutral no-event behavior, canonical packed fields, every
  production action ID, and a four-player team forfeit mask/result pair.
- The 31,463-byte replay now has corpus SHA-256
  `08955dbf44be5e54f229796b42342904ba6933b4ad5779e61c515b71fe1a62fa`,
  final-state SHA-256
  `17bfd5133e5926221fd71d526f2bbb62359a8a36b8c44949d92954137e25a5e2`,
  and event-journal SHA-256
  `ad1be8cd1b341cef74f23b39edd511124fb7515cafb36c81bee6ac87ff8e6a28`.
  Windows and WSL verifier self-tests repeat match-soak digest
  `192e1b210a16982e`.
- Exact clean commit `58d9e5487c062242add32a3330d85f5b540d3e9d`
  passes Windows MSVC and WSL GCC 13.3 Release 22/22, every strict M2/M4 source
  gate, pinned Emscripten 6.0.3, byte-identical native/WebAssembly replay
  output, and the live generated-Wasm Microsoft Edge DOM smoke.
- Two clean native-Windows milestone runs compare the final commit first to the
  latest exact `91d69f5` pre-journal repeat and then to itself. All 10
  measurable scenarios are compatible in both runs with zero invalid,
  suspected, or confirmed regressions. A clean WSL Tracy 0.13.1 capture passes
  with timer fallback enabled; its 11,808-byte trace has SHA-256
  `a5a8879cc6d299eb16dd99dfa2a4b178250ab053290f5145cfd8a3e801c772d3`,
  while `perf` remains unavailable because it is not installed.

## Explicitly preserved playtest requirements

- Keyboard clients must emit reduced horizontal magnitude for slow walk and
  full magnitude for dash/dash-dance. They must also emit reduced vertical
  magnitude so shield platform drop remains distinct from full-down spot dodge.
- A fresh grounded Taunt edge must enter the authored locked recovery while
  preserving dash momentum. At a facing support edge, the existing teeter
  transition must cancel it; away from the edge, all 90 ticks must complete.
- A right-ledge jump must reach the raised block wall. Fresh full-away input at
  contact must enter the authored normal wall jump without consuming the saved
  air jump, and either jump or aerial input must cancel its action window.
  Holding away too early must miss the wall and cannot manufacture the route.
- The pale upper deck above the raised block must permit ascent from below,
  catch a descending fighter on support 5 without horizontal carry, and accept
  both ordinary down drop-through and reduced-down shield drop. The moving
  center platform remains the carrying comparison.
- Jump release during jump squat selects one short-hop speed; holding through
  jump squat selects one full-hop speed. Hold duration after launch does not
  change either height.
- A fresh special edge fires at most one Pulse Bolt from the lowest requesting
  controller slot. Short-hop fire must recover into ordinary air movement and
  generic landing; a fresh two-frame shield reflects it, while an earlier
  shield uses the ordinary block path.
- Down plus a fresh special edge must select Prism Burst instead of Pulse Bolt.
  Its active physical hit must launch downward, its active box must transfer
  projectile ownership without powershield, and invalid or held input must not
  manufacture either action.
- Full up plus a fresh special edge must start Vector Ascent from the ground or
  air and restore ordinary movement after landing lag. Holding light with the
  grounded chord must start or resume Arc Reservoir. Early shield release
  during store must preserve charge and regain the grounded router, holding
  shield through the boundary must enter ordinary shield, Attack must release
  scaled damage, and a hit during charge/store must clear the value.
- Airborne horizontal input changes drift velocity but never changes facing;
  an opposite-direction air jump likewise preserves the takeoff-facing
  direction.
- A fresh airborne trigger produces a neutral or directional air dodge.
  Diagonal surface contact preserves horizontal momentum through exactly ten
  special-landing ticks, enabling wavedash and waveland with keyboard input.
- In the ledge-cancel fixture, that retained landing slide must cross the
  platform support bound before recovery ends and enter `AIRBORNE`; the same
  route at platform center must remain locked for all ten landing ticks.
- During an aerial, that same fresh trigger arms the independent seven-frame
  L-cancel timer instead of replacing the attack with an air dodge.
- When the defender is in an eligible airborne action, a clean full-trigger
  edge at age 0–2 reduces both launch components to 95% without shortening
  hitstun. Age 3, an active aerial, grounded state, or a repeated edge inside
  the 40-tick lockout must not reduce launch.
- The strong-attack input remains usable airborne as the conspicuous L-cancel
  drill: 30 normal landing ticks versus 15 after an eligible trigger, with a
  red/green browser result cue.
- On the ground, trigger plus a fresh full horizontal direction enters a
  forward/backward roll relative to facing; trigger plus fresh down enters
  spot dodge and takes priority. Neither option changes facing.
- Opposite input during initial dash enters the measured one-frame smash
  `STANDING TURN`; holding it then enters the opposite initial dash. Opposite
  input after entering `RUN` must instead enter `RUN TURNAROUND`; neutral or
  sub-threshold run input enters `RUN BRAKE`.
- Repeated same-direction full-input edges separated by neutral releases remain
  initial-dash fox-trot bursts. Holding the direction reaches `RUN`, and using
  the reduced walk magnitude after release cannot restart the dash.
- A one-tick opposite full input followed by neutral or a ground action remains
  the pivot route in `STANDING TURN`. Holding the reversal enters opposite
  initial dash, while waiting until `RUN` produces `RUN TURNAROUND` rather than
  an empty pivot.
- Down from an unlocked run enters `RUN BRAKE`; down from RunBrake remains the
  sliding crouch-cancel route. Jump and shield remain the other live dash
  cancels. Initial-dash shield and run-turnaround crouch input remain rejected.
- A one-tick shield tap from run retains the held shield stop's traction path,
  but enters release as soon as the eight-tick minimum completes. Holding the
  trigger remains `SHIELD`, while the same tap from idle has no travel.
- Full direction plus light attack from idle or walk must enter the matching
  charged smash. Holding light charges through tick 60; releasing early or
  reaching the cap starts the directional strong. Delaying forward light by
  one to three same-direction initial-dash ticks remains the range-extending
  small-step route, while frame 4 must remain the forward tilt.
- Following the opponent's airborne path and observed ground-tech direction
  remains the tech-chase route. The punish begins only when the 20-tick gold
  invulnerability ring clears; attacking from the original spacing at that
  same action tick must miss a directional roll.
- Standing just outside the responder's jab range but inside strong-attack
  range must permit the ordinary whiff counter. Moving closer must let the jab
  hit first, while moving farther must make the strong counter miss too.
- The supporting approach must begin at the default neutral spawn and reach
  those bands through reduced-stick walking; a pre-positioned-only result does
  not satisfy the tactical route.
- Platform sharking must begin with the attacker in legal floor space below a
  target supported by the pass-through platform. A too-early aerial must whiff,
  while the correctly timed route must damage either the target or held shield.
- The neutral-aerial cross-up must begin in front while facing away, pass the held
  shield through air drift, and finish behind with facing preserved. The early
  attempt must whiff and the forward-facing control must remain in front.
- The mindgame route must reuse the same readable approach cue and branch only
  through legal player/responder input. The jab and held-shield responses must
  reward different continuations, while both wrong reads remain visible.
- The juggling route must record at least two legal hits with no grounded
  target state between them. Directional influence plus a fresh air dodge must
  make the attempted airborne follow-up visibly active but non-connecting.
- The ladder route must use multiple upward aerials and an airborne finisher,
  carry the defender above the pass-through platform without an actionable
  frame, take the stock through the upper blast line, and show an active
  outward-DI follow-up whiff before the defender escapes.
- The kill-confirm route must keep the defender in hitlag or hitstun from the
  setup through the neutral-DI finisher, attribute the resulting high-percent
  KO to the attacker, preserve the low-percent non-KO control, and show an
  active whiff for the outward-DI escape.
- The zero-to-death route must begin at exactly 0%, keep the defender in
  hitlag or hitstun from first contact through the typed KO, and show that the
  declared outward-DI policy breaks the sequence into an actionable escape
  before the finisher while a later active attack whiffs.
- Full-up plus fresh Special while airborne must enter the original
  `VECTOR_ASCENT` recovery once per airtime, apply the authored launch and
  steering under ordinary gravity, then enter `FALL_SPECIAL`. A second attempt
  before landing or ledge grab must fail; landing, ledge grab, respawn, and
  reset restore the resource. The recovery byte must survive save/load and be
  visible through inspection plus structured and compact RL observations.

## New binding M4.4 scope

- The governing plan now pins and enumerates all 61 unique techniques marked
  available for SSBM in the referenced advanced-technique table.
- This incremental slice does not claim full technique parity. Dash-dancing is
  verified; approach, auto-canceling, camping, cross-up, dash canceling, dashing shield, drop cancel, edge dashing, edge
  hopping, fox-trotting, gimp, infinite, instant double jump, double jump cancel, double jump cancel counter, L-cancelling, pivoting, SHFFL,
  boost grab, chain grab, jab cancel, juggling, jump-canceled grab, kill confirm, ladder, ledge-cancelling,
  charge storage canceling, mindgame, moonwalk, planking, Scar Jump, shield platform dropping, Shine spike, short hop air dodge, short hop laser, small step forward smash, Stage humping, Stage spike, stalling, taunt cancelling, Team wobble, teeter cancel,
  sharking, spacing, tech-chasing, turtling, V-cancelling, jump-cancelling, and wavedash are
  now playable, as is the zero-to-death combo. No row remains below playable;
  full M4 acceptance still requires every row to advance to `verified`.
- A versioned row-by-row registry, deterministic evidence links, and browser
  playtest recipes are required for all 61 rows before M4 can be accepted; none
  may be deferred to a later milestone.
- Registry schema 1 now exists at
  [`m4_advanced_technique_registry.md`](../product/m4_advanced_technique_registry.md)
  and is mechanically checked for all 61 ordered rows. Its current gate is
  blocked: 1 verified, 60 playable, 0 primitive-ready, and 0 planned.
- M4 must include narrow production-path item, team, projectile, charge,
  reflector-like, shield, grab/throw, aerial, and ledge fixtures wherever the
  non-character-specific registry needs them.
- Character-specific SSBM advanced techniques are a separate M8 fighter-wave
  gate and are not counted among these 61 M4 rows.

## Remaining M4.1 work

- Representative real-hardware confirmation for the temporary browser
  presentation's standard-gamepad mapping; two hot-plug-polled gamepad slots,
  two keyboard slots, and explicit analog walk/dash controls are implemented.

## Remaining M4.2 and M4.3 work

- Character-specific move breadth, additional specials, broader recovery
  options, broader throw routes, and broader per-action launch-angle data.
- Repeated human matches.
- The mandatory owner combat playtest; the generated browser worksheet is
  ready, but only the owner can supply and approve its evidence.

## First-slice verification

- Current compatibility checkpoint `fb5d6be`: Windows MSVC Release passes
  22/22 tests, WSL GCC 13.3 address/undefined-behavior sanitizers pass 22/22,
  and the headless shared-library workflow passes 17/17.
- The current Gymnasium 1.3 adapter passes all six API/determinism tests against
  RL schema 13, transition schema 11, and the 102-value compact schema 12. The
  batched boundary is 6.0566 times faster than repeated single-environment
  calls in the local WSL qualification.
- Setup, benchmark-history, verifier-lifecycle, and Tracy qualification pass at
  the same checkpoint. Synthetic benchmark history now contains ten comparison
  records in every qualified category, and Tracy 0.13.1 captures through the
  supported WSL timer fallback.
- Mechanical oracles: 349 movement invariants including upper-platform
  geometry and drop-through behavior, Moonwalk timing,
  Teeter-cancel, Taunt-cancel, Stage-humping, and Scar-Jump routes and controls, and mid-action
  save/load, plus Vector Ascent data, consumption, restoration, and RL routes;
  982
  attack/reaction/shield/floor/surface
  invariants including data-defined pummels, crouch cancel, victim weight, and
  stale-move queue scaling/registration,
  complete directional ground normals, canonical smash charge, the
  five-direction light-aerial vocabulary, exact shield size/tilt/poke geometry,
  horizontal shield SDI/ASDI,
  ledge attack, and ledge-roll invulnerability plus 74
  combat-journal invariants,
  24 stock/respawn/result
  invariants plus 62 match-journal and 24 moving-revival invariants,
  46 projectile invariants including short-hop laser, projectile camping, and
  powershield reflection,
  32 reflector invariants including Shine spike and active-box projectile
  reflection, 28 charge invariants including storage cancel, exact resume,
  scaled release, interruption loss, and over-cap load rejection,
  and separate 20,000-tick deterministic four-player traces.
- M2 kernel compatibility: movement, snapshot, RL, replay, and forbidden-symbol
  checks passed after the state-schema migration.
- Native replay corpus: exact 180-tick
  attack/reaction/shield/ground-dodge/air-dodge trace at 31,463
  bytes,
  replay SHA-256
  `08955dbf44be5e54f229796b42342904ba6933b4ad5779e61c515b71fe1a62fa`,
  final SHA-256
  `17bfd5133e5926221fd71d526f2bbb62359a8a36b8c44949d92954137e25a5e2`,
  and event-journal SHA-256
  `ad1be8cd1b341cef74f23b39edd511124fb7515cafb36c81bee6ac87ff8e6a28`;
  Windows, WSL, and pinned Emscripten 6.0.3 output is byte-identical.
- Windows and WSL verifier self-tests repeat match-soak digest
  `192e1b210a16982e`.
- The generated-Wasm canonical replay inspector and live playtest pass the
  headless Microsoft Edge DOM gate.

## Browser-adapter verification

- Strict-warning native adapter contract: pass
  (`walk_axis=13500`, `dash_axis=32767`,
  movement/instant-double-jump/double-jump-cancel/double-jump-cancel-counter/bat-drop/glide-toss/jump-cancel-throw/jump-cancel/fox-trot/moonwalk/teeter-cancel/Taunt-cancel/Stage-humping/Scar-Jump/Team-Wobble/pivot-dash-cancel/dashing-shield/small-step-forward-smash/drop-cancel/V-cancel/approach/spacing/sharking/cross-up/mindgame/juggling/ladder/kill-confirm/zero-to-death/ledge-cancel/planking-and-stalling/jump-canceled-grab/boost-grab/jab-cancel/jab-reset/directional-throw-and-chain-grab/
  edge-hop-and-dash/
  ground-dodge-and-roll/air-facing/
  air-dodge-and-wavedash/
  aerial-auto-cancel-and-L-cancel/strong-aerial-30-vs-15-landing/short-hop-laser/projectile-camping-and-turtling/Shine-spike/charge-storage/Vector-Ascent/
  combat-and-event-journal/reaction-and-crouch-cancel/shield-PSC-and-shield-break/default-tumble/
  floor-recovery/tech-chase/surface-tech
  /stock-revival probes and live rendering).
- Browser-standard gamepad mapping/polling contract: pass for synthetic mapping,
  axis quantization/dead zone, D-pad override, button routes, non-standard
  rejection, two-slot assignment, and live `navigator.getGamepads()` polling;
  representative hardware remains an owner check.
- Address/undefined-behavior sanitizer adapter contract: pass.
- Emscripten 6.0.3 build and native/WebAssembly replay comparison: pass; the
  generated client also passes the live DOM smoke in headless Microsoft Edge.
- Browser JavaScript syntax and M1 source-boundary checks: pass.
- Generated owner-evidence contract: pass for registry schema 1/source revision
  2048934, exactly 61 live recipe rows, local draft persistence, filtering,
  rubric/match gates, and Markdown/JSON export. No synthetic result remains in
  the delivered page; human execution is still pending.
- Focused owner controls and expected results:
  [`M4_browser_playtest.md`](M4_browser_playtest.md).
- Generated-page execution: pass in the clean Chrome CI lane.

M5 content scaling remains blocked until M4 combat feel is approved.

## 2026-08-03 Falcon movement-fidelity pass

- The owner authorized provenance-recorded numeric frame tables. The pass uses
  the owner's NTSC 1.02 `GALE01` data and pinned `doldecomp/melee` revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7`; extracted files remain outside
  the repository.
- Content schema 58/fighter schema 50 maps Falcon walk, dash, run, jump,
  double-jump, gravity, air-drift, fast-fall, run-brake, shield-break,
  ledge-jump, wall-jump/tech, ceiling-tech, and landing values into the
  laboratory stage coordinate scale.
- State schema 51/save format 50 added canonical X/Y tilt directions and ages.
  State schema 52/save format 51 added the
  distinct directional aerial-landing and L-cancel action semantics needed for
  Falcon's 15/19/18/15/24 landing-lag table.
- State schema 53 retains save format 51 and 787 bytes while adding the
  Dolphin-measured held-dash transition, released-dash completion, smash-turn,
  basic-standing-turn, and run-brake timing state.
- State schema 54/save format 52 carries the subsequent shield-input,
  shield-health, defensive-route, ledge-transition, and replay-visible state
  needed by the expanded executable-oracle corpus.
- Ground and air acceleration follow the decomp's friction-aware target and
  overshoot branches, including its conditional absolute-speed cap. Initial
  dash applies Falcon's impulse once, and double jump replaces horizontal
  velocity from current stick input.
- Emergent-tactic verifier routines are skipped per owner direction. Core
  deterministic mechanics, save/load, replay, browser, and cross-platform
  checks remain required. The live fidelity audit records partial and divergent
  systems; no whole-simulation equivalence claim is made.
- At this revision, the 240-tick replay had corpus SHA-256
  `30ab31b9c38c7f34c8d81324a40547db84b64353ddfbe6d8ca6602e2b0c31c2b`,
  final-state SHA-256
  `71bfda9f3448a5c140e1654578ad730806f4aad6b0f84bc0bb5eda6ddbed7e7c`,
  and event-journal SHA-256
  `12c446555a8e4b81e544d762a9c066003f509f9859a7d8ba6afb5b5fab95db71`.
- Local Windows MSVC and WSL Linux GCC each pass all 22 CTest targets. The
  repeated-match verifier digest is `8040f1d3de670dca` after the aerial-fidelity
  correction.
- The 1,519-frame Dolphin/Slippi capture and native comparator pass exact action,
  facing, and velocity gates plus the documented accumulated float-to-Q16.16
  maneuver-local float-to-Q16.16 position tolerance. The corpus covers the
  locomotion regression routes plus
  full/light shield, trigger dead-zone rejection, forward/backward/C-stick
  rolls, spot dodge, C-stick spot-dodge and jump buffers, jump from held L into
  a fresh-R air dodge, analog light shield in air without air dodge, nonzero-
  velocity dash/turn, jump-squat reversal, short/full hop, neutral-stick double
  jump, fast fall, and landing.
- PC-mode Mayflash detection accepts neutral controllers whose unpressed
  trigger axes report `-1`, and the browser advances at most one simulation
  tick per animation frame so one physical Gamepad sample cannot age through
  several catch-up ticks before it is rendered.
- Pinned Emscripten 6.0.3 rebuild and live Chrome requalification pass for this
  exact revision. Native and Wasm replay outputs are byte-identical, and the
  page on port 8002 reports every readiness field as pass with no page error.

## Active exact-equivalence gate

- M4 fidelity work is simultaneously subject to the 2026-08-04 implementation-
  quality gate: each shared SSBM formula and transition must have one canonical
  zero-cost C implementation. A route-specific duplicate used only to make a
  Dolphin comparison pass is not acceptable production architecture.

- The owner requires all implemented movement and shared-simulation behavior
  with an intended SSBM counterpart to be exactly equivalent to the NTSC 1.02
  executable behavior. Decomp review and imported frame-data tables guide the
  implementation, but do not replace executable comparison.
- Qualification replays the same ordered, per-frame controller samples in
  Dolphin and this simulator and compares corresponding state and motion. This
  applies to nonzero as well as zero starting momentum and extends beyond
  locomotion to jump/landing, shield/light shield, rolls, spot dodge, air
  dodge, collision/ledges, hit reactions, DI/SDI, teching, stale moves, stocks,
  respawn, and match-state behavior wherever an SSBM counterpart is intended.
- The passing 8,675-frame movement/defense/aerial/crouch/RunBrake trace is a
  regression slice,
  not completion of this gate. Every uncovered applicable route and every
  owner-observed divergence must become a pinned identical-input differential
  reproducer. M4 remains unfinished, and fidelity work continues without
  waiting for CI, until the complete applicable corpus has no unresolved
  divergence.
- Completion is not bounded by the current corpus or by owner-reported bugs.
  The active audit must systematically discover every remaining applicable
  state, transition, analog threshold, timer, and momentum condition from the
  decomp, then qualify it by replaying the same ordered per-frame inputs in
  Dolphin and the simulator. Passing captures prove only the routes they cover.

## 2026-08-04 Falcon common-movement executable-oracle slice

- The identical-input Dolphin route now contains 5,311 frames. The added
  full-down grounded sequence observes seven displayed `CROUCH_START`/`Squat`
  frames, a held `CROUCH`/`SquatWait`, ten displayed
  `CROUCH_END`/`SquatRv` frames, then ground idle. Native comparison passes
  exact action and action-tick gates for the complete sequence.
- Exact `ftCommonData` entry/release values 0.6875/0.625 are now separate
  deterministic thresholds. The corpus proves that exact entry does not squat,
  just-beyond entry does, exact release remains held, and just-beyond release
  starts `SquatRv`.
- The route now also proves jump interruption from `Squat`, `SquatWait`, and
  `SquatRv`; a fresh opposite dash from `SquatWait` produces one displayed
  `Turn` frame before `Dash`; and forward horizontal input during `SquatRv`
  enters `Walk` immediately. The simulator routes those transitions through
  its already-pinned common jump, turn/dash, and walk machinery.
- Fresh digital guard from `Squat`, `SquatWait`, and `SquatRv` enters the same
  `GuardReflect`/shield-start path and then ordinary held shield/release. The
  existing common guard router passes all three executable routes; focused
  deterministic assertions now pin that availability.
- Fresh D-pad-up from `Squat`, `SquatWait`, and `SquatRv` enters `AppealS` on
  displayed frame 1. Dolphin holds Captain Falcon in that action for exactly
  60 displayed frames; production now uses terminal counter 61 to account for
  its input-tick action update, and focused assertions pin all three entries.
- Fresh D-pad-up on displayed frame 2 of an ordinary `Turn` first applies the
  executable's facing flip and then enters `AppealS`. Production's existing
  pre-router facing update already had the correct order; the common taunt
  eligibility list now includes `STANDING_TURN` and a focused assertion pins
  the action, tick, and facing result.
- Falcon's normal `Landing` exposes the common interrupt router immediately
  after displayed frame 4. The zero-based production timer previously waited
  one extra frame; one shared predicate now translates it to the displayed
  boundary. Short-hop routes pin `Landing` frames 1-4 followed by frame-1
  `AppealS`, `KneeBend`, dash/turn, guard, walk, direct `SquatWait`, or ordinary
  `Turn`, as selected by the executable's common IASA ordering.
- The landing crouch route found and corrected a distinct transition: down on
  the first legal frame enters `SquatWait` directly, without replaying the
  seven-frame `Squat` animation. The decomp permits that check only while the
  current frame is below `normal_landing_lag + frame_speed_mul`; an explicit
  negative route proves down one displayed frame later remains in `Landing`.
- Main-stick tap jump now uses the imported common 0.6625 threshold and
  four-tick vertical-tilt window from `ftCo_Jump_GetInput`. Identical-input
  routes pin held-stick full hop, released-stick short hop, air double jump,
  first-legal-frame Landing jump, and shield jump. Boundary controls prove
  just-below rejection, just-above acceptance, a slow sweep aged out at four
  ticks, and a two-sample sweep accepted inside the window.
- Decomp review at pinned revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` confirms that `Squat` and
  `SquatWait` are crouch-cancel eligible while `SquatRv` is not. Attack,
  special, grab, and platform-pass portions of the interrupt
  matrix remain uncovered and therefore remain active work under the
  whole-simulation gate.
- Content schema 62/fighter schema 54 validate, hash, and default the seven-tick
  crouch entry, ten-tick reverse timing, distinct crouch release threshold,
  independent tap-jump threshold/window fields, three-tick platform-pass
  startup, and executable-derived pass-entry speed.
  State schema 55 names the replay-visible action vocabulary; save format 52
  remains 787 bytes.
- A deterministic trace exposed and fixed an independent transition invariant:
  entering `GRABBED` now clears charge and smash-charge counters so action-local
  state cannot leak across capture.
- An initially contaminated dash route also exposed Falcon/Fox ground push
  displacement near X=+60. The route was relocated and recaptured away from
  both Fox and the ledge so it does not misattribute push displacement to
  locomotion. Player push collision remains an explicit uncovered shared-
  simulation route rather than being silently accepted as movement drift.
- The current 240-tick replay corpus SHA-256 is
  `93e60fef3c6afcac94d66b96ac4a29dd5257c39617700969447e47fe51c8278f`,
  final-state SHA-256 is
  `61160ef3e40848b4e5e529a27f81a0658938152bf6e3e4b0acb7395d32d2890e`,
  and event-journal SHA-256 is
  `f574b8063f8339b8495ec44eaea0a0c09395c1bf5f545dc5e4454248baeb62ba`.
  The repeated-match verifier digest is `b6a774204dedd4a7` after the pinned
  Falcon RunBrake executable-oracle correction.
- Windows MSVC and WSL Linux GCC each pass all 22 CTest targets after the
  schema/fixture refresh. The clean Emscripten 6.0.3 build has byte-identical
  native/Wasm replay output, the rebuilt live browser page reports every
  startup probe passing with no console warnings/errors, and the standalone
  replay/kernel workflows pass. M4 remains
  unfinished after publication because the executable-oracle corpus still has
  uncovered applicable routes.

## 2026-08-04 Falcon RunBrake common-IASA executable-oracle slice

- The identical-input Dolphin route now contains 7,128 frames. Its RunBrake
  matrix follows pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7`: common RunBrake IASA contains
  jump, animation-command TurnRun, and crouch only.
- Neutral from terminal run enters displayed RunBrake frame 1 at Falcon ground
  velocity 2.22. Jump or main-stick down on the next sample enters displayed
  `KneeBend` or `Squat` frame 1 at velocity 2.06. A one-frame down tap now
  proceeds directly from displayed `Squat` frame 7 to `SquatRv` frame 1 when
  released; production previously exposed one extra `SquatWait` tick.
- Opposite stick on displayed RunBrake frame 2 enters displayed TurnRun frame
  1, preserves the old facing, resumes the brake animation cursor, and applies
  Falcon's 0.16 TurnRun acceleration. Production previously held RunBrake
  until animation expiry and entered ordinary `Turn`.
- Fresh neutral guard, C-stick roll, C-stick spot-dodge, D-pad taunt, A, Z, and
  B do not interrupt RunBrake. Full guard plus main-stick down still enters
  `Squat`, because crouch is the independent third IASA branch. Focused native
  assertions additionally reject strong attack, canonical grab, main-stick
  roll, and both C-stick defensive directions.
- Capture schema 3 records A, B, Z, and their post-frame observations. It also
  records Slippi's implicit 0.35 analog shoulder value for physical Z, while
  the comparator reproduces the project's device-normalized full-trigger-plus-
  A canonical grab packet. Every non-neutral C-stick sample similarly carries
  the production strong-attack bit.
- Two independently exposed physics gaps are corrected in the same expanded
  trace. Released-dash completion selects Wait's high-speed friction from the
  velocity entering the frame, and normal Landing uses Falcon's high-speed
  grounded friction while absolute velocity remains above walk maximum.
- The plain-FD comparison runner now disables the original Relay Rod, matching
  the Dolphin match's items-off setup and preventing original content from
  contaminating common-movement evidence. The separately tracked player-push
  collision gap remains uncovered rather than being hidden by a position
  tolerance or fabricated landing root motion.
- Browser reaction, crouch-cancel, weight, shield-hit, and powershield probes
  no longer attempt their attacks from RunBrake. They use a bounded walk to
  jab range, settle in Wait, and then exercise the same combat invariants from
  a legal SSBM action route. The live smoke now makes reaction, shield,
  shield-break, and powershield-cancel probe success mandatory.
- Windows MSVC Release and WSL Linux GCC each pass all 22 CTest targets. The
  strict movement, combat, M2 kernel, and standalone replay workflows pass;
  the 7,128-frame comparator passes exactly against both native binaries.
  Pinned Emscripten 6.0.3 produces byte-identical replay output, and the
  rebuilt live Chrome DOM/Wasm smoke passes with all startup probes green.
- The refreshed 240-tick replay corpus SHA-256 is
  `93e60fef3c6afcac94d66b96ac4a29dd5257c39617700969447e47fe51c8278f`,
  final-state SHA-256 is
  `61160ef3e40848b4e5e529a27f81a0658938152bf6e3e4b0acb7395d32d2890e`,
  event-journal SHA-256 is
  `f574b8063f8339b8495ec44eaea0a0c09395c1bf5f545dc5e4454248baeb62ba`,
  and the repeated-match verifier digest is `b6a774204dedd4a7`.

## 2026-08-04 Falcon crouch common-IASA executable-oracle slice

- The identical-input Dolphin route now contains 8,675 frames. Twelve new routes
  exercise neutral A, neutral B, physical Z, and down-special from displayed
  `Squat`, `SquatWait`, and `SquatRv` states.
- Neutral A enters displayed `Attack11` frame 1 from all three states. The
  project's corresponding semantic action is `GROUND ATTACK`; focused core
  regressions pin all three entries.
- Neutral B enters Falcon's neutral-special action from `Squat`, but does not
  interrupt `SquatWait` or `SquatRv`. Production previously allowed the
  projectile-fire action from all three states. Neutral projectile eligibility
  is now limited to `CROUCH START` among the crouch actions, and focused tests
  pin both rejected routes.
- Dolphin reports physical Z as the Z bit plus an approximately 0.35 analog
  shoulder value. `Squat` runs the common catch check and enters displayed
  `Catch` frame 1. `SquatWait` and `SquatRv` do not expose catch; their same Z
  packets fall through the A component and enter displayed `Attack11` frame 1.
  Production now blocks A only when grab can actually start and prevents the
  unavailable catch chord from being misrouted to shield.
- Pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` confirms the asymmetry: `Squat`
  contains neutral special and catch checks, while `SquatWait` and `SquatRv`
  do not. The latter two retain their attack checks.
- Three further routes prove that down-special is accepted from `Squat`,
  `SquatWait`, and `SquatRv`, matching the shared `ftCo_800D68C0` check in all
  three decomp IASA lists. Production already exposed the correct reflector
  semantic action; focused core regressions now pin every entry.
- Falcon Kick's character-specific travel is isolated at the end of the
  corpus. Each route first lands and settles, creates space away from the FD
  edge, and faces back toward center. The project oracle runner enables its
  original reflector counterpart on a widened plain floor, then compares only
  entry eligibility. No tolerance was widened. Platform-pass was the sole
  crouch-IASA route still uncovered by this particular capture.
- Attack and special bodies remain original project content. The differential
  runner therefore compares their common semantic eligibility at entry,
  advances past the character-specific body, and resumes exact action, facing,
  velocity, position, and applicable action-tick comparison at the next
  stationary anchor. Rejected routes remain exact for their whole duration.
- The 8,675-frame comparator passes against both Windows MSVC and WSL Linux
  GCC binaries. All 22 CTest targets pass on both platforms. Replay corpus,
  final-state, event-journal, and verifier expectations remain unchanged
  because the fixed eligibility routes are outside the pinned 240-tick replay.

## 2026-08-04 Falcon platform collision/pass executable-oracle slice

- A separate 348-frame identical-input capture selects Battlefield, where
  Captain Falcon starts on the left pass-through platform. It is intentionally
  isolated from the 8,675-frame Final Destination corpus and from the original
  laboratory-stage geometry.
- A neutral jump first proves upward pass-through, the complete Falcon arc, and
  ordinary descending collision. Dolphin remains in `JumpF` for one frame after
  the first descending crossing, then enters `Landing` for 30 displayed frames.
  Production previously landed on the crossing frame; ordinary-airborne
  platform collision now preserves the executable's final airborne frame.
- The negative control applies down for one frame and then releases. Dolphin
  remains grounded through ordinary `Squat`/`SquatRv` and never latches a
  delayed pass; production now follows the same route.
- The positive control holds down. Dolphin exposes displayed `Squat` frames
  1-3 and enters `Pass` frame 0 on the following sample. Production previously
  dropped immediately from pass-through support; the authored three-tick
  startup now reproduces the executable transition.
- Pinned decomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7`
  confirms `ftCo_8009A228` assigns common-data field `x46C` on Pass entry. The
  executable value 0.63 converts to Q16.16 value 7,325 in the laboratory scale.
  Entry applies that velocity without an additional gravity or fast-fall step.
- With the Battlefield platform-to-floor displacement represented exactly in
  the comparison fixture, held-down Pass lands on the solid floor on Dolphin's
  frame 140. The comparator does not widen action, velocity, or position
  tolerance to obtain this result.
- Content schema 62/fighter schema 54 hash and validate the platform-pass
  startup and speed. State schema 55/save format 52 remain unchanged. Focused
  deterministic tests pin validation, content-hash sensitivity, release and
  held controls, upper-platform behavior, and shield-drop reuse of the pass
  speed.
- The 240-tick replay corpus, final-state, and event-journal hashes remain
  `93e60fef3c6afcac94d66b96ac4a29dd5257c39617700969447e47fe51c8278f`,
  `61160ef3e40848b4e5e529a27f81a0658938152bf6e3e4b0acb7395d32d2890e`,
  and `f574b8063f8339b8495ec44eaea0a0c09395c1bf5f545dc5e4454248baeb62ba`.
  The content-sensitive repeated-match verifier digest is
  `d3b2847f6de12e56` after the ordinary platform-landing correction.
- The 348-frame Battlefield comparator and the 8,675-frame Final Destination
  comparator pass on Windows MSVC and WSL Linux GCC. All 22 CTest targets pass
  on both platforms; strict movement/combat, standalone kernel/replay, and
  native-versus-Wasm replay checks pass. A clean pinned Emscripten 6.0.3 build,
  the rebuilt live port-8002 playtest, and headless Chrome DOM/Wasm smoke also
  pass. M4 remains unfinished because player push collision and the other
  unqualified shared-simulation routes remain active under the exhaustive
  exact-equivalence gate.

## 2026-08-04 Falcon grounded player-push executable-oracle slice

- A separate 540-frame identical-input Final Destination capture has each
  Captain Falcon approach the other in turn, covering both horizontal
  directions and both controller ports. Its SHA-256 is
  `fae58079659be55a5e74f57a9772cf2c51c244b79ba6ebbf7fe92fe9721a8f09`;
  the reproducible capture remains outside the repository with the extracted
  disc data. Capture schema 5 records the second port's ordered per-frame
  horizontal input.
- Pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` identifies
  `ftCommon_8007DD7C`/`ftCommon_8007E0E4` as the grounded-fighter nudge path.
  The owner's NTSC 1.02 `PlCa.dat` gives Falcon `x2C4=(0.0, 3.5)`, and
  `PlCo.dat` common field `x450` is 0.3. Production converts the radius to
  `42/115` and nudge to `18/575` project units.
- The comparator records and checks both players' action/state, action frame,
  facing, grounded state, position, and self-induced velocity. Discrete values
  remain strict. Per owner direction, the push route accepts the ordinary 640
  Q16.16 position envelope plus at most one mapped 0.3-unit nudge (2,052), for
  a reported 2,692-Q16 bound when fixed-point accumulation delays the strict
  float overlap by one tick. It does not widen action or velocity comparison.
- Falcon's `51/575` walk maximum now uses nearest Q16 encoding, and normalized
  axis multiplication rounds to nearest. The prior truncation accumulated a
  one-unit-per-tick deficit and selected the wrong strict-overlap branch after
  a long held walk. The 8,675-frame movement/defense/crouch corpus and
  348-frame platform corpus continue to pass with their unchanged 640-Q16
  position bound.
- Content schema 63/fighter schema 55 hash, validate, and default the player
  push radius and speed. State schema 55/save format 52 remain unchanged.
  Focused deterministic coverage pins symmetric displacement, unchanged
  velocity, strict separation, and invalid content values; fixtures that
  intentionally overlapped players were updated to legal non-overlap setups.
- The refreshed 240-tick replay corpus SHA-256 is
  `d40e33283f43e7f77f93a281aafd33300b855349851984d65e13cd520d306ee5`,
  final-state SHA-256 is
  `9ca9f8efbb1756d021cbf04cb89b65540300315236f762f312d729c4b65146cf`,
  event-journal SHA-256 remains
  `f574b8063f8339b8495ec44eaea0a0c09395c1bf5f545dc5e4454248baeb62ba`,
  and the content-sensitive repeated-match verifier digest is
  `2123cbc83b968f35`.
- Windows MSVC Release and WSL Linux GCC each pass all 22 CTest targets and all
  three identical-input comparators. Strict movement, combat, M2 kernel, and
  replay workflows pass. Pinned Emscripten 6.0.3 produces byte-identical replay
  output; the rebuilt live port-8002 playtest and headless Chrome DOM/Wasm smoke
  pass. M4 remains unfinished because other unqualified shared-simulation
  routes remain active under the exhaustive exact-equivalence gate.

## 2026-08-04 Falcon analog shield-pressure executable-oracle slice

- A separate 500-frame identical-input Final Destination capture samples
  requested shoulder pressures 0.29, 0.30, 0.35, 0.40, 0.65, and 0.99, both
  shoulder ports, simultaneous 0.40/0.70 shoulders, a digital full click,
  release, and regeneration. The first reliably represented light sample is
  requested 0.35 and reported by Slippi as 0.3214286. Its SHA-256 is
  `051c6fdb9185b513a6505c6cd93a6b3de6659455613e68ae21f4c8bd5eaa0572`.
- The pinned decomp `ftCo_Guard.c` route and freshly extracted owner-disc
  `PlCo.dat` values record 60 start health, 30 reset health, 0.30 analog dead
  zone, 0.14 base hold drain, 0.1/2.0 drain-density endpoints, 0.07
  regeneration, 0.15 minimum size, 1.0/0.5 pressure-size endpoints, and 0.5
  guard-stick smoothing. No extracted DAT is retained in the repository.
- Windows and WSL comparators agree for action/state, shield health, and
  normalized pressure across all 500 frames. Health retains its existing
  64-Q16 conversion bound; normalized pressure accepts one 16-bit unit. The
  pre-existing 8,675-, 348-, and 540-frame captures continue to pass with
  their documented bounds.
- This slice introduces no production or schema change because the input,
  hold-depletion, regeneration, and health/density size constants were already
  data-defined. Exact tilt smoothing, executable shield geometry, collision
  damage, shield stun, and attacker/defender pushback remain uncovered and M4
  remains unfinished.

## 2026-08-04 Falcon shield-hit response executable-oracle slice

- Three separate 283-frame identical-input Final Destination captures drive
  Falcon jab into light, intermediate, and dense Falcon shield. Requested /
  Slippi-observed pressure is 0.35 / 0.321428567, 0.65 / 0.592857122, and 1.0 /
  0.914285719. Capture SHA-256 values are
  `563cabf633126656b80a0351b67fdffb35f664774e052e85c04ff7b20fd2e4f5`,
  `84b462f717074b2a2984b6901ed33a2abd2b9f98527f1c52db400c98ace411ab`,
  and `2d95549b7ffe6ac950c339fe9dcd346b4e6c401324d2cce0e8414d2677a3489f`.
- Pinned decomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7`
  and the owner's extracted `PlCo.dat` map integer shield-hit damage `D` and
  normalized pressure `p` to health damage `D*(0.9-0.2p)`, stun duration
  `D*(1.425-0.975p)+2`, defender pushback `duration*0.2*0.6` capped at 2,
  and attacker recoil `p*D*0.07+0.02`. Powershield defender pushback omits
  the 0.6 factor. No extracted disc data is committed.
- Attacker recoil is canonical state separate from ordinary self velocity. It
  decays before integration by Falcon ground friction times 1.1 or by 0.05 in
  air. A single shared shield-response helper and a single shared application
  path serve physical attacks, items, and projectiles; the hot path remains
  allocation-free and the compiler can inline the response arithmetic.
- The comparator checks both fighters' action, facing, grounded state,
  position, self velocity, shield health/pressure, hitlag, and shield stun. It
  independently infers executable attacker recoil as position delta minus self
  velocity and compares it directly to the simulator component. Discrete gates
  are exact, recoil/velocity use 32 Q16 units, and position retains the 640-Q16
  float-to-fixed envelope. All three captures pass against Windows MSVC and
  WSL Linux GCC runners.
- Post-hitlag shield stun now updates physics on the same tick as the decomp:
  ASDI and defender pushback both affect the first resumed frame. Grounded
  shield SDI/ASDI past a support edge enters fall instead of clamping to the
  edge. Focused core coverage pins pressure endpoints/midpoint, separate recoil
  decay/integration, and browser probe ordering without adding tests for
  emergent techniques.
- Content schema 64/fighter schema 56, state schema 56/save format 53,
  inspection schema 48, the 663-byte canonical payload, and 803-byte
  checkpoint make the new data and state fail closed. The refreshed 41,575-byte
  replay SHA-256 is
  `20081466dda33520e122343f6ad178d685cf6a0b398a04e598408cf41a6d03f3`;
  final-state SHA-256 is
  `f6db91a5b18186515ad09609aa353b7ab6fd69da01c753eeec44dca404d43ef4`;
  event-journal SHA-256 remains
  `f574b8063f8339b8495ec44eaea0a0c09395c1bf5f545dc5e4454248baeb62ba`,
  and the repeated-match verifier digest is `c824a2207a625170`.
- The aggregate executable-oracle evidence is now 10,912 captured frames.
  Windows MSVC and WSL Linux each pass all 22 CTest targets; strict movement,
  combat, M2 kernel, native/Wasm replay equality, browser adapter, and real
  headless-Chrome playtest smoke also pass. Optimized MSVC code generation
  emits one shared application routine called by the three collision sources;
  its response and pressure interpolation helpers are inlined, and the
  zero-recoil movement path is guarded before decay arithmetic. A derived
  sparse-component presence mask also keeps inactive ticks from copying or
  committing the recoil array and is rebuilt from canonical values on load.
  Exact shield tilt/geometry and every uncaptured shared-simulation route stay
  active under the exhaustive near-equivalence and implementation-quality
  gates; M4 remains unfinished.

## 2026-08-04 Falcon shield tilt and geometry executable-oracle slice

- A 270-frame cardinal/diagonal route and a 2,158-frame angular sweep use
  Dolphin Memory Engine to record live GALE01 guard magnitude, biased guard
  angle, shield-joint translation/scale/world matrix, fighter position, health,
  and analog pressure. Capture schema 6 records those probes alongside the
  existing post-frame controller and Slippi state. Capture SHA-256 values are
  `02b420230efdaf105889c73ec413ff459eadbf98103a4a6a6dea0dacfa49e92f` and
  `fb90e6173feb98139019ddd98eda05390bbf7ed38ebad662b1eedb2f1c22f9f0`.
- Pinned decomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7`
  confirms shortest-wrapped angle smoothing and magnitude smoothing by common
  factor 0.5. The executable probes resolve Falcon's direction animation as
  eight exactly linear 45-degree segments, including the encoded 0.799926758
  and -1.799804688 lower keys, rather than guessed symmetric offsets.
- Canonical shield steering remains four bytes per player: an unsigned turn
  and Q0.16 magnitude replace the former unsmoothed Cartesian pair. A compact
  65-entry octant table supplies deterministic fixed-point `atan2`; one shared
  eight-key table supplies the joint animation. Public Cartesian tilt is
  derived only at inspection boundaries.
- One allocation-free shield-volume authority computes center and anisotropic
  radii from content, health, pressure, facing, angle, and magnitude. Inspection
  and browser visualization consume its bounding box, while physical collision
  uses its ellipse against the authored hitbox. The formula is not duplicated
  across attacks, items, projectiles, replay, RL, or web code.
- Falcon's 15-unit initial radius, 0.97 model scale, neutral joint center, and
  independent 12/115 x and 11/62 y conversions are content-hashed. Content
  schema 65/fighter schema 57, state schema 57, observation schema 13,
  inspection schema 49, and save format 54 fail closed on the changed data and
  canonical state meaning; checkpoint size
  remains 803 bytes because the canonical steering state remains four bytes.
- The comparator explicitly accounts for the one-frame controller/post-frame
  pipeline difference for guard state while retaining same-frame health/radius.
  Both new captures pass strict action/facing/grounded gates and bounded
  fixed-point angle, magnitude, center, and radius gates. Together with the
  prior routes, aggregate owner-executable evidence is 13,340 frames.
- The refreshed 41,575-byte replay SHA-256 is
  `2c9b2e214c336b95408592b29669c31cf9fc36d7f8fe6714cd35387b8c82bd64`;
  final-state SHA-256 is
  `0b99990e67bd75d868dbf421edbc8a3a1727add168be367c88b11be8a64b52f6`;
  event-journal SHA-256 remains
  `f574b8063f8339b8495ec44eaea0a0c09395c1bf5f545dc5e4454248baeb62ba`,
  and the repeated-match verifier digest is `88983bd051160acd`.
- Windows MSVC Release and WSL Linux GCC each pass all 22 CTest targets. Strict
  movement, combat, M2 kernel, native/Wasm replay equality, browser adapter,
  and real Windows Chrome playtest gates pass; the rebuilt port-8002 playtest
  visibly presents the exported elliptical full shield and its label. An
  alternating clean-c336b56/candidate Windows comparison with 100 ms samples
  and 15 repetitions passes all 10 measurable scenarios with zero invalid,
  suspected, or confirmed regressions. Optimized MSVC objects retain one
  shared shield-volume authority, compact integer lookup tables, and no
  trigonometric or square-root runtime imports.
- M4 remains unfinished: these captures qualify only sampled Falcon shield
  routes, and the exhaustive decomp/inventory gate still contains broader
  shared collision, damage, defensive-action, ledge, and match-state work.

## 2026-08-04 complete Falcon frame-data import and table-routed attack slice

- The owner NTSC 1.02 `PlCa.dat` and `PlCaAJ.dat` were processed with pinned
  `meleeDat2Json` and `meleeFrameDataExtractor` revisions. The resulting
  geometry-free canonical table has SHA-256
  `42bb4ecefb33e87dc978482ecdb7b1f93ff12ca090e870431fff913480601356`
  and covers all 50 ordinary, grab/throw, and special subactions, including
  every active phase and effect. A hash-checking importer generates the compact
  immutable C table; raw DAT and hitbox geometry remain outside the repository.
- Default jab 1/2, dash attack, all three tilts, all three explicit smashes,
  and all five aerial routes now consume generated timing and phase-specific
  damage, angle, KBG, set-weight, BKB, and hitlag. Exact frame lookup preserves
  inactive gaps and late-hit changes; the same table owns aerial landing lag
  and pummel timing/damage. Jab 1's deterministic fixed-point Melee response
  produces 1,179 x and -11,369 y Q16 simulation velocity, three hitlag frames,
  and 13 hitstun frames, corresponding to the captured zero-percent Dolphin
  velocity after the documented independent coordinate conversions. Custom
  jab content still selects semantic or authored-vector knockback explicitly.
- The final 271-frame live-hitbox/damage capture has SHA-256
  `2660274136b77aef393db391c85582be7795bee7360ebd6607325e437ac9af04`.
  Aggregate owner-executable evidence is now 13,611 captured frames.
- Content schema 67 and fighter schema 59 fail closed on the two semantic jab
  records. The generated table is compiled once behind zero-allocation query
  functions, so content defaults, combat frame selection, and tests do not
  duplicate the 50-subaction data.
  The refreshed 41,575-byte replay SHA-256 is
  `0d303a7a8a30fe59f391bf7779e716f193de5fcabf7a9ae026fa4b566aafa028`;
  final-state SHA-256 is
  `2dca650173bd419852de3011d7e75f8887d61b1da8e31688f22c1a912c21905b`;
  event-journal SHA-256 is
  `c9b0f348b2ca91d83ced7c5e2c290847c118ec8c8da936db4fad7a1639660206`,
  and the repeated-match verifier digest is `aa6e1a0e6ba20a35`.
- Windows MSVC Release and WSL Linux GCC each pass all 22 CTest targets. M4
  remains unfinished: simultaneous bone-relative sweet/weak geometry,
  separate standing/dash grab states, then-unrouted production throw effects,
  ordinary knockback decay, DI, and broader collision coverage remain active
  fidelity gaps. The following source-decoded throw slice resolves the throw
  response item. Original generic strong-aerial and special fixtures are not
  presented as Falcon-equivalent behavior.
- A clean alternating Windows comparison against pre-import commit `4b6ea8c`
  used 100 ms samples and 15 repetitions. Candidate run 4 in
  `performance/local/falcon-frame-data-alternating-20260804.sqlite3` passes all
  10 measurable scenarios with zero invalid, suspected, or confirmed
  regressions; the first pair's isolated empty-tick suspicion did not reproduce
  while the candidate binary remained unchanged.
- Clean table-routed commit `6eb6d2302f196b6e1a25c59ceabc16dfd88ba55a`
  is run 5 in the same database, again using 100 ms samples and 15
  repetitions. All 10 measurable scenarios compare compatibly with zero
  invalid, suspected, or confirmed regressions. Representative 1v1, 2v2, and
  maximum-combat medians are 1,694,588, 1,111,536, and 993,063 logical ticks
  per second. The separate unsampled 64-environment Python boundary reports
  321,157 repeated-call ticks/s versus 1,688,386 batched ticks/s, a 5.2572x
  boundary speedup.

## 2026-08-04 source-decoded Falcon throw slice

- The full owner DAT JSON is now a second pinned importer input with SHA-256
  `fa18647a5d94826429ef6f961461e66118dcb18e0a30fa124d1bbf03c6476266`.
  The importer decodes the original action-script waits and encoded opcode
  `0x14` argument rather than trusting the upstream `reverseDirection` label.
  The NTSC 1.02 decomp shows argument zero raises the victim-release flag.
- Forward, back, up, and down throw therefore release on exact action frames
  18, 20, 15, and 20 and finish on frames 39, 49, 43, and 39. Their production
  damage and angle/KBG/WDKB/BKB records are 4/45/105/0/11,
  4/135/130/0/7, 3/85/105/0/17, and 7/65/34/0/18 respectively.
  Defaults and runtime response query the generated table; no throw value is
  transcribed into `sim_content.c`.
- One shared `pf_m4_melee_knockback_data` value selects the zero-allocation
  integer Melee response for attacks and throws. Explicit custom content can
  disable it and use the pre-existing vector response. Content schema 68 and
  fighter schema 60 hash and validate the choice without a hot-loop lookup or
  duplicate formula.
- The focused 20,000-tick combat verifier passes with exact throw startup,
  hitlag, recovery, stale-move, chain-grab, and ordinary team resolution.
  Emergent team-handoff/wobble testing remains skipped by owner direction. The
  repeated-match digest changes to `138363409704b533`. Remaining Falcon gaps
  include throw collateral hitboxes, transformed bone geometry, specials, and
  the broader audit inventory.

## 2026-08-04 table-routed standing and dash grab slice

- The importer already retained Falcon's separate `Catch` and `CatchDash`
  subactions. Production now consumes their exact 5/2/22 and 9/2/28
  startup/active/recovery schedules instead of the former guessed 4/2/10
  compromise.
- `DASH_GRAB` is a distinct action state. The decomp's Dash IASA routes grab to
  `CatchDash`, so initial dash and run inputs, plus the existing boost-grab
  cancel, select it; ordinary grounded and jump-cancel grabs retain `GRAB`.
  One branch selects the immutable timing fields for action completion and
  grabbox activation, with no per-tick allocation or duplicated frame table.
- The generated table test fixes both source schedules, the content hash covers
  all six timing fields, and validation rejects incomplete schedules. Basic
  team targeting remains covered; emergent team wobble/handoff is deliberately
  not maintained as a regression test.
- The new action identifier raises state schema 57 to 58 while retaining save
  format 54 and the 803-byte layout. The refreshed 41,575-byte replay SHA-256
  is `617e8a8503be61670f7683f4478baf0699d49d21182346edb1d80003b130f86e`;
  final-state SHA-256 is
  `80e4140554f0ca0b797d5056049768128cdcd08487341464835a1909812ad4c7`.

## 2026-08-04 table-routed Falcon aerial landing slice

- Neutral, forward, back, up, and down aerial now query their distinct imported
  NTSC 1.02 autocancel intervals rather than sharing the placeholder's authored
  interval. Their landing-lag-active intervals are frames 4-33, 7-34, 7-20,
  1-21, and 4-35 respectively.
- L-cancelled landings use the table's explicit 7/9/9/7/12-frame durations.
  A compact reference-match guard preserves the original content fallback for
  customized fighters; there is no duplicated per-aerial runtime table and no
  allocation or search in the tick path.
- The refreshed 41,575-byte replay, final-state, and event-journal SHA-256
  values are `47bb7eb24cf96c2a5bc46b2b11a83e451226fa0308fdc8d5cdaa43c6a38a9acd`,
  `9fef35cd32abe0fa98013bf32c1f98d953289551465aa5f2dffec60834bb5f56`,
  and `b92e065007f6794dc1d26bb38313c185775ac14833f02d2f951fe9ba3a2aab18`.
  The repeated-match verifier digest is `957a947eaff53872`.

## 2026-08-04 table-routed Falcon Jab 1 IASA slice

- The production Jab 1 route now reads imported IASA frame 16 from the complete
  Falcon table. A shared pure timing-and-damage matcher preserves custom-content
  fallback, and the tick path adds no allocation, lookup-table duplication, or
  character-data transcription.
- Pinned decomp callback `ftCo_Attack11_IASA` exposes jump followed by
  dash/crouch/turn/walk, but not guard, grab, special, or taunt. Production
  implements only the proven intersection shared by the currently mapped
  ground callbacks; action-specific branches remain an explicit follow-up
  instead of being incorrectly generalized to `ftCo_Wait_IASA`.
- A 543-frame owner-executable capture proves that frame-15 jump stays locked,
  frame-16 jump enters `KneeBend`, guard stays locked on both boundaries, a
  pre-held horizontal stick enters Walk, displayed frame 21 remains visible,
  and the next tick returns to Wait. The identical-input comparison passes at
  the established 640-Q16 position tolerance. Capture SHA-256 is
  `d17f8e9a7dfc3c1a0d260d3ffe3c7fd9c3e2b5f89f3f17b1c3ce9e7218a8f427`;
  aggregate owner-executable evidence is now 14,154 captured frames.
- The refreshed 41,575-byte replay, final-state, and event-journal SHA-256
  values are `1149ddd6bfd08048ff48c833a736ce6d023f975a718351b9db9d399177cb2af1`,
  `1bcf9444a66610479106ff4bc0782e891365a1a36e4561513fe47e1777b5272a`,
  and `61d93989ea9c831cd2cf562787fd978cb9e7bd05f694c87671e9520ec26ae280`.
  The repeated-match verifier digest is `1bbde902bfb8d7ce`.

## 2026-08-04 complete grounded-normal data and callback slice

- The pinned Falcon importer now verifies raw `PlCa.dat` and `PlCaAJ.dat` in
  addition to both JSON views. It retains all 48 concrete subactions' action flags and
  decodes every compressed translation-N animation track into an immutable
  per-frame Q16 table; no attack movement speed is guessed or copied into
  authored defaults.
- Grounded normal physics follows the pinned decomp callback split. Jab, dash
  attack, and forward smash consume animation root motion, while tilts and
  up/down smash apply ground friction. Dash attack is legal after Falcon's
  early forward-smash dash window and its focused 511-frame identical-input
  Dolphin trace now matches action, position, and velocity.
- Imported IASA frames select four compact callback policies: Jab chain, Wait,
  down tilt, and forward smash without escape. The policies reuse existing
  zero-allocation action handlers and also cover down tilt's crouch exit and
  the neutral-special preprocessor boundary. The full executable matrix and
  refreshed replay hashes are recorded after final qualification below.
- The refreshed 41,575-byte replay, final-state, and event-journal SHA-256
  values are `478547e440e1fcc274760a9d6c0bdfbd62286438f27b2f448702cb6af9a3f03e`,
  `eda4430b3f2623afe857cafbf39929e81d87ae05d7b2128c68ceace2803f6c4b`,
  and `ffe86482a586401206fc75c01d8ecc959ab48e6ed053350d261352a19ddc25ca`.
  The repeated-match verifier digest is `35292518eb393fa2`.
- The completed grounded-normal/IASA executable matrix is a 5,450-frame
  identical-input Dolphin capture with SHA-256
  `3596f20946bc6e8bd629ec875442857e8986fca6a69fc7a530a8ed6630cc24b1`.
  Action, position, and velocity all match within the fixed-point-only position
  allowance of 640 Q16 units. Qualification exposed and corrected two real
  state-machine gaps: forward-smash recognition now uses main-stick input age
  rather than Dash action age, and a backwards horizontal A press falls through
  to Jab instead of becoming a reverse forward tilt. Neither result is encoded
  as a trace-specific exception.
- This raises aggregate owner-executable evidence from 14,154 to 19,604
  captured frames. Windows MSVC and WSL Linux GCC each pass all 22 CTest
  targets, the Emscripten web client rebuilds cleanly, and both native builds
  pass the complete 13-scenario profiler workload. The WSL unsampled headless
  run reports 1,531,353 single-world and 1,638,910 batched ticks per second
  with identical state. Canonical performance-history qualification is still
  pending because this local WSL compiler is GCC 13.4 while the repository's
  measurement contract requires GCC 13.3 exactly.

## 2026-08-04 executable-qualified Falcon hit-geometry slice

- The complete owner-extracted frame-data source regenerates byte-for-byte as
  a 50-slot schema with 48 concrete Falcon subactions. The only absent rows are
  the two forward-smash angles not defined by Falcon's NTSC 1.02 DAT; all 17
  character-special rows are present. Timing, effects, throws, landing data,
  IASA, action flags, and root motion remain generated rather than transcribed.
- A hash-pinned 1,719-row Dolphin 3.4.0 memory capture adds live transformed
  geometry for every currently implemented normal and aerial: 117 active-frame
  rows, 240 independent attack spheres, and the 11-capsule standing hurt pose.
  The importer cross-checks each sphere's effect against the complete static
  table and rejects mismatched disc, capture, extractor, or decomp provenance.
- Production collision now performs source-sphere versus capsule/rectangle
  narrow phase and selects each simultaneous sphere's own damage and knockback
  effect. Lookup is O(1), storage is fixed capacity, and the tick path performs
  no allocation. The inspection ABI and web overlay expose the same spheres;
  custom authored fixtures opt out explicitly instead of defeating source
  geometry with magic values.
- The canonical replay keeps its SDI coverage with an explicit opposite-stick
  sample on input tick 113, immediately after the source-geometry hit. Its
  refreshed 41,575-byte replay, final-state, and event-journal SHA-256 values
  are `4f38617b574c30088ff374283918ddf5e89111d417d638bf164120908126caed`,
  `34d2019a582d081cce10b8c7053909b8b5153d45cfe3d889f41bd7f135fc29ac`,
  and `6a58ca0e2ef8dd3e08308d8b8d3085c22c73530400286013f305f2343f38bf87`.
  The refreshed eight-match verifier digest is `23545e83e9dbef9c`.
- Remaining geometry gaps are explicit: animated hurt capsules outside grounded
  idle, specials/grabs, source-Z collision semantics, and exact
  sphere-versus-shield intersection. They require extraction and executable
  qualification, not guessed frame data.
- Fresh release verification passes 20/20 configured native Windows GCC tests,
  21/21 WSL Linux GCC tests, a clean Emscripten web build, and byte-identical
  native/Wasm canonical replay output. The Windows throughput executable now
  samples `QueryPerformanceCounter` rather than relying on MinGW's incomplete
  C `timespec_get` surface. A separate unsampled WSL run reports 1,181,323
  single-world and 1,281,076 batched ticks per second with identical state;
  the independently sampled 13-scenario profile workload passes with ten
  scenarios available. These local numbers are evidence for this build, not a
  canonical historical comparison because the compiler contract differs.

## 2026-08-04 source-qualified standing and dash grab geometry

- The full owner-extracted Falcon source remains the authority: all 50 schema
  slots are validated, all 48 DAT-defined subactions are retained, and only
  Falcon's genuinely absent mid-high/mid-low forward-smash variants are empty.
  The production reference path does not synthesize missing timing or effects.
- A new 1,933-row Dolphin 3.4.0 memory capture, SHA-256
  `5a7ac3a35775b0352d48566d622860c846fa2907c4bef03f760080f2a18ba3e8`,
  adds the live standing-grab and dash-grab routes. The generated geometry now
  contains 121 frame rows and 250 spheres: standing grab has two spheres on
  executable frames 7-8 and dash grab has three on frames 11-12.
- Both attacks and grabs reuse one fixed-capacity source-sphere transform and
  one bounds reducer. Grab narrow phase uses exact sphere-versus-capsule
  intersection and rejects live Falcon hurt capsules whose source `grabbable`
  bit is clear. The authored rectangle remains only the explicit custom-content
  fallback.
- Every isolated geometry route resets the opponent through the same short
  action and settle sequence before capture, pinning the standing pose instead
  of inheriting an arbitrary menu/match idle phase. An independent recapture
  produced byte-identical converted numeric rows after the provenance comments;
  unused raw float variation therefore does not enter the Q16.16 tables.
- The generated include regenerates byte-for-byte with SHA-256
  `fbfd5184ea7b67917d09ec125283ebba5228e383bd09f190fceaa1d3f1b652f9`.
  The focused WSL combat verifier passes its 20,000-tick deterministic run and
  all combat fixtures. Fresh Windows GCC and WSL GCC builds pass 20/20 and
  22/22 configured tests respectively; the Emscripten web client and browser
  adapter verifier pass as well. Native/Wasm replay output remains identical at
  corpus/final/event SHA-256
  `4f38617b574c30088ff374283918ddf5e89111d417d638bf164120908126caed`,
  `34d2019a582d081cce10b8c7053909b8b5153d45cfe3d889f41bd7f135fc29ac`,
  and `6a58ca0e2ef8dd3e08308d8b8d3085c22c73530400286013f305f2343f38bf87`.
  The 13-scenario sampled workload passes with ten scenarios available. A
  separate unsampled WSL run reports 992,316 single-world and 997,311 batched
  ticks per second with identical state; these local compiler results are not
  substituted for the repository's canonical performance history.

## 2026-08-04 complete-frame Falcon hurt-pose geometry

- A new schema-9 Dolphin 3.4.0 memory capture records the acting Falcon's 11
  live `FighterHurtCapsule` records on every displayed frame of the 14 routed
  normals/aerials and both grabs. The 1,948-row capture SHA-256 is
  `d9fea72b7eb86447e5bd53b2157ec7f3dde9a27f02a28750ec4964ab6bd7ef32`.
  Its full-hop route delays the double jump by two ascent samples, keeping
  neutral air and down air airborne through source frame 44. The importer
  rejects a fractional, missing, duplicated-but-different, wrong-facing, or
  wrong-action pose rather than manufacturing a replacement.
- The immutable output adds 612 frame rows and 6,732 capsules. One move-indexed
  offset and one frame-indexed offset select a contiguous, at-most-11-capsule
  span; the collision path performs no allocation. The original independently
  pinned 121 hit-frame/250-sphere capture remains the attack-position source,
  and its numeric section regenerates byte-identically despite the new aerial
  setup. Grounded idle retains its separately pinned Stand frame-18 pose.
- A shared hurt-pose selector now supplies source sphere collision, grab
  collision with the live `grabbable` bit, and authored item/projectile bounds.
  It uses the existing canonical action/action-tick state and follows
  `hitlag_resume_action` so an attacking Falcon's pose freezes during hitlag.
  There is no per-action runtime branch table or new rollback state.
- The combined canonical geometry SHA-256 is
  `e04cb094239f43d449d95da11b84c75918354816f87f2a8ebcb7b29e09e3743e`;
  the generated include SHA-256 is
  `5376ee7efc2d92e6c2cd478d6996d896f826301d92441bd12a686f16d03d113e`.
  Content schema 69/fighter schema 62 folds the geometry digest into the M4
  content hash. The eight-match verifier identity correspondingly refreshes to
  `40a4c37827908f55`; save/state schemas and the 803-byte checkpoint do not
  change.
- Fresh verification passes 20/20 Windows GCC tests, 22/22 WSL GCC tests, the
  Emscripten build, browser adapter, Windows-Chrome Wasm smoke, and identical
  native/Wasm canonical replay hashes
  `4f38617b574c30088ff374283918ddf5e89111d417d638bf164120908126caed`,
  `34d2019a582d081cce10b8c7053909b8b5153d45cfe3d889f41bd7f135fc29ac`,
  and `6a58ca0e2ef8dd3e08308d8b8d3085c22c73530400286013f305f2343f38bf87`.
  The 13-scenario profile workload passes with ten available scenarios. A
  separate unsampled WSL run reports 1,406,579 single-world and 1,426,551
  batched ticks per second with identical state.
- M4 remains unfinished. Captured hurt poses outside these 16 actions, special
  hit geometry and executable frame-rate logic, source-Z collision, exact
  sphere-versus-shield intersection, and normal-throw collateral hits remain
  active source/decomp work rather than guessed-frame placeholders.

## 2026-08-04 complete Falcon source attributes and special geometry

- The pinned raw `PlCa.dat` import now preserves all 97 common-attribute words
  and the full 0x8c-byte, 35-field `ftCaptain_DatAttrs` special block alongside
  the existing 48 concrete action records. A generated typed Q16.16 view
  supplies every Falcon common attribute currently consumed by default content,
  replacing the remaining duplicated movement/jump/fall/weight/landing ratios.
  The combined action, attribute, and provenance identity is
  `616461670890a22878a37e891b848808f3633d1b9f236226f6dd35044f7a8946`;
  the generated frame-data include SHA-256 is
  `f05deb23cff37fb71165e39576d4e1cbe6ddbb298fa7041fb3744275af56e832`.
- Hash-pinned Dolphin routes now cover every displayed frame of all 17 Falcon
  special subactions: ground/air Falcon Punch, Raptor Boost start/hit, Falcon
  Dive start/catch/throw, and every Falcon Kick ground/air/end/rebound state.
  Every damaging or grabbing phase has live transformed sphere geometry; the
  non-damaging Raptor Boost search volumes are deliberately not reclassified as
  attacks. The complete canonical geometry identity is
  `92f5014de753bf5660e5f4eb566e4e92ac734871089f359c697fa9a3d8e6b4c0`;
  the generated geometry include SHA-256 is
  `d9be4319b7a416df24d9c11a23c5ac6a1f6134acbdad9afecfe22741922f6294`.
- Importers hard-reject wrong raw DAT hashes, timing-table hashes, capture
  hashes, action IDs, incomplete displayed-frame ranges, and non-Q16-equivalent
  duplicate samples. The final Falcon Kick wall-rebound motion aliases the
  Falcon Dive throw pose because the pinned DAT motion-state table itself uses
  that animation; no replacement frame data is authored.
- Fresh release verification passes 20/20 Windows tests and 22/22 WSL Linux
  tests. The Emscripten client rebuilds cleanly, and native/Wasm replay output
  is byte-identical at corpus/final/event SHA-256
  `9d40ce0f622f748e85ebc663cc1394d531b6757ee66ef3538287a85576756a6b`,
  `863a4ed6b2f6334f6b67c15cf8c219d93ef84c8300792d6820db304dfa2e8e23`,
  and `6a58ca0e2ef8dd3e08308d8b8d3085c22c73530400286013f305f2343f38bf87`.
  The eight-match verifier identity is `2b88c65c0067185c` on Windows and WSL.
- M4 was still unfinished at this checkpoint: imported Falcon specials had not
  yet been production-routed. Common-state poses, source-Z collision, exact
  sphere-versus-shield intersection, and normal-throw collateral hits also
  remained active fidelity work.

## 2026-08-04 source-routed Falcon Punch

- Default reference content now selects distinct ground and air Falcon Punch
  actions instead of the original Pulse Bolt. Custom projectile, reflector,
  charge, and emergent-technique fixtures explicitly opt out; they do not pose
  as Falcon-equivalence evidence.
- The importer decodes the `SpecialAirN` command-variable timeline directly:
  launch and velocity scaling begin on displayed frame 50, scaling ends on
  frame 64, ordinary air physics resumes on frame 65, and the imported
  animation ends on frame 99. Production reads these generated values and the
  five Falcon Punch special attributes instead of duplicating constants.
- Ground Falcon Punch consumes its imported per-frame root translation. Air
  Falcon Punch consumes source stick-angle launch, velocity scaling, gravity,
  and air control with axis-specific Melee-to-stage conversion. Positive source
  stick Y is mapped to negative screen Y. Ground/air collision transitions
  retain the action frame, and neutral special is legal from `Squat` but not
  from `SquatWait` or `SquatRv`, matching the source common dispatcher.
- Combat queries the captured complete-frame pose and simultaneous hit spheres
  for both actions. Default reference input cannot also spawn a Pulse Bolt, and
  the web adapter exposes the new canonical action names.
- Direct Dolphin captures are pinned at SHA-256
  `2c8bc604024cfad745e266239dcc4d3e1b1ff1c4a07afcc6eecb9938b5f155b1`
  for the 200-frame ground route and
  `9cfc8c5632a8bce37a0f79c6999bff6f0742130df5f8f2473196338d8b14d6c5`
  for the 241-frame air-physics route. The latter observes launch `(1.794, 0)`
  on frame 50, multiplier physics through frame 64, ordinary physics on frame
  65, and `Fall` after frame 99.
- `tools/verify_m4_falcon_punch.sh` strictly rebuilds the native movement
  runner, verifies the GALE01 NTSC 1.02 disc SHA-256, and differentially checks
  200 frames of each route on demand. Both routes pass with a 640-Q16 position
  allowance and 32-Q16 velocity allowance. Windows passes 20/20 CTest targets,
  WSL Linux passes 22/22, pinned Emscripten rebuilds cleanly, native/Wasm replay
  output is byte-identical, and the refreshed port-8002 page reports
  `playtest=ready` with no failed probes or browser-console diagnostics.
- The generated frame-data include SHA-256 is now
  `e24f1e5093f55942da2429550e95044ec80e0b1e04d50305ef84be0942cc1f8d`.
  Raptor Boost, Falcon Dive, and Falcon Kick remain the next source-routing
  slices; no guessed move values are accepted as substitutes.

## 2026-08-04 complete Falcon regeneration and source-routed Raptor Boost

- The Falcon import is complete and reproducible from the pinned NTSC 1.02
  inputs: all 50 stable move slots, all 48 DAT-present subactions, the two
  explicitly absent forward-smash angle variants, all 97 common-attribute
  words, all 35 character-special attributes, action scripts, command-variable
  timelines, effects, throws, animation translation/root motion, hit/grab
  spheres, and complete-frame hurt poses. The complete generated source digest
  is `96bafb1940d737a0e912377ac2893c1a3877b10078ed8e06b81c3f8a0fb36326`;
  the canonical geometry digest is
  `f7bb98902431ed7c7bb80b689cb3e954b5e8c2f40a7793b8dd8a81deafe75052`.
  Fresh regeneration byte-matches both checked-in includes, whose SHA-256
  values are `755200a1f70674475bf2775c02e2e1581b47222a798bf191c7c503f43bd23556`
  and `6a0b7a3e74d1a1bdf760994c004aebce1926fa56fe33bf0acb7eecbc91ff59a4`.
- Default reference side special now routes source ground/air Raptor Boost.
  It consumes the imported 0.6 selection and 0.2 turnaround thresholds,
  ground velocity multiplier, root translation, frames 15-34/18-34 search
  windows, six distinct search spheres, frame-30 air-gravity command, 7-damage
  ground/air hit effects, miss/hit landing lag, and all four start/hit actions.
  The source search resolver is shared, fixed-capacity, and allocation-free;
  custom special/projectile fixtures retain an explicit reference-data opt-out.
- The new at-will `tools/verify_m4_raptor_boost.sh` runner drives the owner
  executable and simulator with identical inputs. Its 46-frame ground-hit
  route passes strict action and velocity checks plus the established 640-Q16
  position envelope. This trace exposed a shared one-frame fidelity defect:
  after hitlag reached zero, restored actions waited one extra simulation tick.
  The production path now resumes ordinary movement and physics on that same
  tick, matching Dolphin; affected deterministic fixtures and replay identities
  were updated to the corrected behavior rather than preserving the error.
- WSL Linux passes all 22 configured tests. Native and WebAssembly replay
  output is byte-identical at corpus/final/event SHA-256
  `e3359756020e844f973406666cc87874389374c4d42689d9a52a67eba63d941d`,
  `40fc23de68fe634ff70b7919e663a55780a3fa2e00e4ee60f8aec1d7b8008e60`,
  and `ddc1f793a4d9919988f4f44f6a78d7492a37b0f4721867f7f1f8ca5bb89ce2d7`.
  The refreshed eight-match verifier digest is `223018a366b0af14`.
- M4 remains unfinished. Falcon Dive and Falcon Kick still require production
  routing and identical-input qualification. Raptor Boost air, miss/edge, and
  item-search routes also remain explicit executable-oracle work; no guessed
  timing or geometry is accepted in their place.

## 2026-08-04 source-routed Falcon Dive and complete-source revalidation

- Default reference up special now routes the imported ground/air Falcon Dive
  start, catch, and throw states instead of Vector Ascent. Production consumes
  the generated command timelines, special attributes, root translation,
  grab spheres, complete-frame hurt poses, 5% catch effect, and 12% throw
  effect; custom reference-data opt-outs retain the original fixture. The
  source route is not gated by Vector Ascent's original once-per-airtime
  resource: the pinned `ftCa_SpecialHi` entry has no such availability flag.
- The capture probe now records both fighters' live ECB top, bottom, right, and
  left points from `fighter+0x794`. The hash-pinned 146-row grounded route has
  SHA-256
  `4518dbb5cd43158baeaa1ddad7d5ffd073b4dda46ecbe2aa55d8c7efa9eadfdb`
  and supplies the source Falling bottom plus the grounded Catch-to-Throw
  relocation. The source ECB is routed only through the floor-contact path it
  describes; mixing it with the original rectangle's solid side/top extents
  is forbidden until those complete collision poses are imported.
- The identical-input comparator passes all 116 comparable frames with strict
  action, facing, grounded, capture-link, hitlag, and velocity checks and the
  existing 640-Q16 position envelope. This includes catch, holder/victim
  attachment, throw release, damage, root motion, falling, and floor landing.
- Fresh regeneration from all five pinned inputs reports exactly 50 schema
  slots and 48 present subactions and byte-matches the checked-in include at
  SHA-256
  `ba0b94cff4ad16d6ca606a926d6df87c311b6ff24a4247b56a9562d1d804a046`.
  The complete generated-source digest is
  `676813b2a18210c445771011668856b9b91fd5a3b2da8728236ee2899ef2ab64`.
  No frame count, effect, attribute, command boundary, or root-motion value is
  guessed.
- Windows and WSL Linux pass all 22 configured tests. The eight-match verifier
  is stable across repeated runs at digest `0a0bff2cdff21b7c`. State schema 59/save
  format 55 fail closed on the newly serialized Falcon Dive action meanings
  without increasing the 803-byte checkpoint. Falcon Kick and the
  uncaptured Falcon Dive/Raptor Boost routes remain explicit M4 work.
- Native replay identity is refreshed at corpus/final/event SHA-256
  `741b5f9451a3a81bf57a632c65e35a5ba35cbbd3fe45e331da4d196b6bfe3847`,
  `f9fb60d1a468e9cd99d43bd412b408d0dfe8c1b414b8a1ef8dfe08f4dea27702`,
  and `ddc1f793a4d9919988f4f44f6a78d7492a37b0f4721867f7f1f8ca5bb89ce2d7`.

## 2026-08-04 source-routed Falcon Kick and no-guessed-frame enforcement

- Default reference down special now routes the seven imported Falcon Kick
  states: ground/air start, ground end, ground-origin air end, air-origin end,
  landing hit, and wall rebound. Prism Burst remains available only through
  explicit custom-content reference opt-out and is no longer presented as
  Falcon behavior in the web playtest.
- Fresh regeneration consumes all five pinned source inputs and reports 50
  stable slots with all 48 DAT-present Falcon subactions. The generated
  canonical source SHA-256 is
  `42bb4ecefb33e87dc978482ecdb7b1f93ff12ca090e870431fff913480601356`;
  the complete-source digest is
  `39fc2a489460791b5557442063361873709afd22c51f59835433edef4b4274a2`;
  and the checked-in generated include SHA-256 is
  `498ea9566f82051ed88ca6b8cf43e3e84c4f8a57994f90e91ec3c63a7be57e50`.
- Production decodes and consumes the source command-variable writes at the
  wall-rebound, traction, edge-fall, and ordinary-air-physics boundaries. It
  consumes the imported 0.6 on-hit speed multiplier, five-application cap,
  ground/landing traction, common fast-ground-friction multiplier, all seven
  root-motion rows, and captured hit/hurt geometry. No authored move timing or
  replacement frame value is used.
- The pinned 170-row Dolphin capture SHA-256 is
  `6244baaf1354749a118a3577f3ca080f87dc4ba59d60f14b947077922a667a2d`.
  Its 70 comparable action frames pass identical-input differential checks for
  action, action tick, facing, grounded state, velocity, and position within
  the established 640-Q16 representation envelope and 32-Q16 velocity
  envelope. The run exposed and corrected a one-frame ground-end traction
  offset; the decoded source command frame remains the authority.
  `tools/verify_m4_falcon_kick.sh` rebuilds and reruns this oracle at will.
- State schema 60/save format 56 serialize the bounded per-player ground-hit
  counter. The canonical payload is 667 bytes and the checkpoint is 807 bytes.
  Replay identity is corpus/final/event SHA-256
  `84fe270389ef33e6b39d2ea7afcee0435f1b5b731f36e47e2bdb02b62a5d207a`,
  `3f9275e0a1b0a07e8d9373696783ace52e8b660a9daf0b45d70b0e3e711c2c60`,
  and `ddc1f793a4d9919988f4f44f6a78d7492a37b0f4721867f7f1f8ca5bb89ce2d7`.
  WSL Linux passes 22/22 tests and native Windows passes 20/20; the repeated
  match verifier digest is `a88fe2040d30b79a`.
- M4 remains unfinished. Falcon Kick air, landing, edge, wall-rebound, and
  ground-hit dynamic routes still need strict identical-input qualification;
  broader shared-state and collision gaps remain active rather than inferred
  from the static table.

## 2026-08-04 Falcon Kick air, landing, and edge qualification

- The at-will Falcon Kick verifier now covers 264 comparable frames across the
  pinned ground, air, air-to-ground landing, and ground-to-air edge captures.
  The added capture SHA-256 values are
  `1b72cb23727cd0770ee2fb5c4a7c8e9e17e91c71548e45be57bbe85cb8df5990`,
  `b86de007baeb6048488d4f2aaa258690ef2d09693033fd9b0f78865d81ea80d2`,
  and `ae4d9c2f3cb19af35e9288d5f9dcc9a7a4a72adc1baa220bc3ffe03b8e17ddfc`.
- Those routes exposed source-order details that static frame rows cannot
  express: air-end physics runs on the transition tick, landing frame zero
  preserves incoming vertical velocity, terminal fall speed clamps after
  gravity, and `mpColl_8004B108` commits half of Falcon Kick's edge-crossing
  displacement while preserving full root velocity and applying no same-tick
  gravity. Production now follows those decomp/oracle semantics.
- All four routes pass strict action, action-tick, facing, grounded-state, and
  velocity comparison with the established 640-Q16 position envelope. The
  wall-rebound and ground-hit dynamic routes remain unqualified and M4 remains
  unfinished; no replacement timing is inferred for either route.

## 2026-08-04 Falcon Kick ground-hit qualification

- Falcon's complete NTSC 1.02 source remains the authority: 50 stable action
  slots, all 48 DAT-present subactions, all 97 common-attribute words, the
  complete 35-field special block, raw root tracks, command timelines, effects,
  and full-frame hit/hurt geometry. No Falcon frame value is guessed.
- A new 170-row Dolphin memory capture, SHA-256
  `0dcf3574554a97a4760ff93e51dd24ebeb6d84d8dc5c23e777054ebe46a5ac32`,
  supplies 77 comparable ground-hit frames. It records contact on displayed
  frame 16, 15 damage, eight ticks of hitlag, and the imported 0.6 velocity
  multiplier after hitlag.
- The differential exposed a source-order detail absent from static tables:
  Falcon Kick ground end advances Melee's unscaled ground channel in parallel
  with its scaled self-velocity channel. Production now reconstructs that
  bounded channel from imported root motion, entry scale, common/fast friction,
  traction, multiplier, and cap. The implementation allocates nothing, adds no
  duplicate serialized velocity, and shares the generated Falcon data path.
- `tools/verify_m4_falcon_kick.sh` now qualifies 341 frames across ground, air,
  landing, edge, and ground-hit routes. All five pass strict action, action-tick,
  facing, grounded-state, velocity, attacker/defender hitlag, and defender
  damage checks with the established 640-Q16 position envelope. Fresh source
  regeneration reports 50 slots/48 present subactions and byte-matches both
  checked-in generated includes at SHA-256
  `498ea9566f82051ed88ca6b8cf43e3e84c4f8a57994f90e91ec3c63a7be57e50`
  and `6a0b7a3e74d1a1bdf760994c004aebce1926fa56fe33bf0acb7eecbc91ff59a4`.
- Native Windows MSVC and WSL Linux each pass 22/22 tests. Pinned Emscripten
  rebuilds the web client and replay corpus; the headless browser smoke passes,
  and native/Wasm replay output remains byte-identical. The browser verifier's
  stale pre-schema-60 final-state hash was refreshed to the canonical replay
  value already enforced by the replay verifier.
- Only Falcon Kick's wall-rebound dynamic route remains unqualified; M4 remains
  unfinished.

## 2026-08-04 Falcon Kick wall-rebound qualification

- The complete Falcon dataset remains authoritative and unchanged: 50 stable
  slots, 48 DAT-present subactions, all 97 common-attribute words, the complete
  35-field special block, root tracks, command timelines, effects, and full-
  frame hit/hurt geometry. No wall timing or trajectory value is guessed.
- A source-derived Hyrule Temple fixture selects `St_Kind_Shrine` from
  `MnSlMap.usd` at cursor `(-3.3, 10.1)`. Falcon jumps legitimately, is moved
  while airborne, and lands through Melee's collision callback near the rising
  `GrSh.dat` wall before the unmodified down-special input. The resulting
  253-row capture hashes to
  `fd4b04d9128486d2b690ce7d9b701fa12c7762367d5f822d2f3baca3c3f0d70e`.
- Dolphin enters action 363 after the displayed-frame-22 wall-hug sample. The
  58-frame differential exposed two production gaps: Melee clears `gr_vel` but
  preserves Falcon Kick's `self_vel` on the transition, and its ECB lock avoids
  same-tick floor reattachment. Production now preserves the incoming channel
  and suppresses only that impossible same-tick landing, with no allocation,
  duplicated frame table, or added serialized state.
- `tools/verify_m4_falcon_kick.sh` now passes 399 frames across all six ground,
  air, landing, edge, hit, and wall routes. Falcon Kick has no remaining
  unqualified dynamic state. M4 remains unfinished because broader common
  behavior and remaining Raptor Boost/Falcon Dive dynamic routes still require
  qualification.
- Fresh Windows MSVC and WSL Linux rebuilds each pass 22/22 tests. The pinned
  Emscripten 6.0.3 build, headless Chrome browser verifier, and native/Wasm
  240-tick replay byte-identity check also pass after the wall correction.

## 2026-08-05 complete Falcon data use and aerial Falcon Dive qualification

- The complete Falcon import remains the sole move-data authority: 50 stable
  schema slots, all 48 DAT-present subactions, all 97 common-attribute words,
  the complete 35-field special block, root tracks, command/effect timelines,
  and full-frame hit/hurt geometry. The importer now also decodes
  `PlCo.dat`'s `Fighter_804D6548` table directly. Default content therefore
  uses Melee's exact newest-to-oldest stale reductions of
  9/8/7/6/5/4/3/2/1 percent instead of the former authored approximations.
- The pinned aerial Falcon Dive capture SHA-256 is
  `59a4489ea6e955c9bb587bb5e49bc5d34ce4cce6ae42accd98a24ff97e271a6f`.
  Its memory probe records internal victim damage, applied/latched knockback,
  throw weight and magnitude, and the live damage-state timer. Catch deals 5%,
  the shared move identity is then stale for the 12% throw, and the exact first
  stale slot produces 15.92% total internal damage.
- Decomp plus executable state show `CaptureCaptain` computes the damage-state
  duration and then clears launch velocity in `ftCo_800DE7C0`. Production now
  reproduces the zero-launch path, immediate ordinary gravity, and 26 visible
  post-transition reaction frames. The shared movement path also now resolves
  zero-hitlag reactions instead of silently losing their pending state, and a
  captured fighter updates correctly while airborne through the same bounded
  `GRABBED` path used on the ground.
- `tools/verify_m4_falcon_dive.sh` reruns all four catch/miss captures at will.
  It passes 116 grounded-catch, 92 aerial-catch, 103 grounded-miss, and 165
  aerial-miss frames, plus 42 aerial victim frames that
  strictly compare capture/reaction action, grounded state, internal damage,
  hitstun, and both velocity axes. The legitimate native jump fixture reaches
  its floor for the last three victim samples while the isolated executable
  capture is held at y=500; those samples are not used to hide any move-state
  difference, and the imported 26-frame boundary remains asserted directly.
- The miss captures prove that `FallSpecial` has its own repeating eight-frame
  ECB-bottom cycle. Production consumes that imported array instead of the
  ordinary `Falling` pose, and preserves the ground route's transition-row
  vertical velocity. Ground and aerial capture SHA-256 values are respectively
  `97672ddf0e5013beaad8ff4c31f54c6bae93551ca3a38755cc3d185bcd5b83c4`
  and `9ecf456e6377f5b7d371ccb84c9f5bd7b3a1045724a7c223acb6cb9d4681fd21`.
- Two fresh importer runs are byte-identical. The generated include SHA-256 is
  `d4e3e787b515fb04f05f205133aca2f33ff12308d72763d2c3249132d0c20402`;
  the complete-source digest is
  `6614ac4e4a9956327abeca886ece87e04ac598224c154efeb9981908005021f3`.
  Native Windows MSVC and WSL Linux each pass all 22 configured tests. Pinned
  Emscripten rebuild, headless Chrome smoke, browser-adapter verification, and
  native/Wasm replay byte identity also pass. M4 remains unfinished: Falcon
  Dive edge behavior, remaining Raptor Boost routes, and broader common-
  state fidelity gaps remain active executable-oracle work.

## 2026-08-05 Raptor Boost miss qualification

- The complete Falcon import remains the sole authority for Raptor Boost:
  action scripts, command boundaries, root motion, special attributes, six
  search spheres, effects, and complete-frame pose geometry are generated from
  the pinned NTSC 1.02 inputs. No timing, velocity, pose, or transition value
  was tuned from observation or guessed.
- A fresh clean 431-row Dolphin memory capture contains the grounded- and
  aerial-miss routes at SHA-256
  `81cafb4d75e75c1f876b6a903a770a3e20376d0399d9374cab19d7feea413602`.
  `tools/verify_m4_raptor_boost.sh` now passes 80 grounded-miss and 180 aerial-
  miss frames in addition to the existing 46-frame ground hit. A separate
  aerial-hit capture, SHA-256
  `8eda88a578afb770af4d28a0a166413d2ee3ecf9da38fb533b45012c958e262a`,
  adds 55 frames covering search conversion, imported seven-damage frame-3
  contact, five-frame hitlag, and recovery through displayed frame 33. The
  at-will suite therefore covers 361 strict comparable frames.
- The aerial-hit runner uses ordinary jump input for both fighters; it does
  not mutate fighter state. Its finite-floor fixture reaches ground before
  the y=500 isolated Dolphin capture after frame 33, so the remaining action
  tail and landing/edge conversion are explicitly unclaimed rather than
  hidden behind a tolerance.
- Raptor Boost and Falcon Dive now share one zero-cost predicate for the
  imported eight-frame common `FallSpecial` ECB-bottom cycle. This removes the
  move-specific condition without duplicating the pose table or adding state.
  The aerial miss also applies ordinary imported Falcon gravity on its
  transition row, matching the executable after active special physics ends.
- Corrected transition ordering refreshes replay corpus/final/event SHA-256 to
  `b0f176b41a3031756c236f1827080808c49104c3938e407287653e1557ac0ce9`,
  `e96399e8c83d2e148554d57b4b2287e11316eaa48780e8e6e729f92e47ee7517`,
  and `ff1b77013c60df79c5d130be72f67e37205998038b9983621bd33cd88cd1d253`.
  The refreshed eight-match verifier digest is `416848e8f260cf6c`.
- Aggregate owner-executable evidence is now 14,530 captured frames. M4
  remains unfinished: Raptor Boost aerial-hit tail, edge conversions, and source
  item-search behavior, Falcon Dive edge behavior, and broader shared-state
  fidelity gaps remain active work.

## 2026-08-05 complete Raptor Boost aerial hit and landing route

- A new 192-row natural-floor Dolphin capture has SHA-256
  `f3f8518c103958f6b6e56e76b5bc728c0b3da0854f26a022fac978869f6d051b`.
  One pre-action relocation establishes the same separation as the native
  fixture; after that, both fighters jump through controller input and no
  position or fighter state is held or modified during comparison.
- The former 55-frame aerial-hit slice is replaced by 145 strict frames. They
  qualify frame-18 search conversion, exact imported frame-3 seven-damage
  contact, five-frame hitlag, the complete natural pre-landing hit-state tail,
  natural
  floor contact, all 40 hit-landing-lag ticks, and return to standing. The
  at-will Raptor verifier now covers 502 frames in total.
- The natural route exposed a one-frame-early landing caused by the generic
  body extent. A separate complete memory capture at SHA-256
  `86e0abff2d1de0483e25ef8db045da323a35331bf95fb7089b00283233b4fc8e`
  supplies every displayed frame 0 through 44 of `SpecialAirS`'s live ECB
  bottom. Production consumes that generated 45-entry Q16.16 table through
  the existing collision-pose view and preserves incoming vertical velocity
  on the transition row; no tuned collision constant or duplicated state was
  added.
- Regeneration from all five pinned Falcon inputs byte-matches the generated
  include at SHA-256
  `7d1d7da573f112080ddf4aaecebfc8dd375c3724786f8d844e6dfab496bca1cf`.
  The expanded complete-source digest is
  `e57a62aa980c0895d7e362ff83d45d80f64125ef5f6c2f0a7b07a934fbf8c275`,
  and the refreshed eight-match verifier digest is `083493fa2ea1613e`.
- Aggregate owner-executable evidence is now 14,671 captured frames. Raptor
  Boost still requires source item-search routes;
  Falcon Dive edge behavior and broader common-state fidelity remain active
  M4 work.

## 2026-08-05 Raptor Boost ground-edge conversion

- `ftCa_SpecialSStart_Coll` at pinned doldecomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` establishes the transition: once
  `cmd_vars[2]` is active, floor loss enters common `FallSpecial`, clamps air
  drift, and applies the imported 20-frame miss landing lag.
- A 210-row owner Dolphin capture at SHA-256
  `3b59fbf62ad880ffe88d694e6e69f0a0b08f23fd7e4714c9a0cf4a41194744ae`
  contributes 51 strict comparison frames. Falcon is relocated once before
  the action; the opponent then flees through ordinary controller input and
  neither fighter is held or modified during the compared route.
- The route exposed two production gaps on the exact floor-loss row: the
  frame-20 root velocity was not clamped to Falcon's imported air speed and
  ordinary gravity ran one tick early. One shared integer clamp now implements
  the decomp ordering for both grounded miss and hit edge conversions.
- The route fixture derives its crossing boundary directly from generated
  Raptor Boost frames 1 through 20. No authored timing or movement constant is
  duplicated. The at-will Raptor suite now covers 502 frames and aggregate
  owner-executable evidence is 14,671 frames.

## 2026-08-05 Falcon Dive ledge collision and complete collision-data import

- The complete Falcon source importer now decodes the DAT's entire
  `ftData_x44` collision block in addition to the existing 50 move slots, 48
  present subactions, 97 common words, 35 special fields, scripts, root
  motion, geometry, and stale table. Runtime memory independently confirms
  Falcon's authored ledge-snap values are exactly x=9, y=17, and height=11.
- The pinned native ledge capture SHA-256 is
  `5a5b295d0fc7a8d1c06512dc704176a131a7c01a931a0a2b92f6d7ff8c3a8295`.
  It uses one safe on-stage preposition, then only controller input. Sixty-
  three Falcon Dive frames match action, facing, position, and both velocity
  axes within the bounded 640-Q16 allowance. The source verifier separately
  proves frame 64 enters `EdgeCatch` and frame 71 enters `EdgeHang` using the
  exact decomp inequalities and live collision contact/position/ECB memory.
- Production replaces the generic half-width-plus-air-speed reach during
  Falcon Dive with one zero-allocation data view over the authored ledge-snap
  values and the complete 64-step live ECB right/bottom sequence. The same
  source review corrected two independent gaps: Falcon may change facing only
  on the exact frame-13 command gate, and its horizontal cap uses imported
  `air_drift_max` rather than the separate maximum-air-speed field.
- `tools/verify_m4_falcon_dive.sh` now reruns 539 strict comparable frames and
  the source-verified catch/hang transition at will. Regeneration from all
  five pinned Falcon inputs byte-matches the generated include at SHA-256
  `0be763ee219d385e738e7c32a2cfbd65b9adfbc5759fed114280ce1f9588ca34`;
  the expanded complete-source digest is
  `af9020d9a33ccfe37fb0fa86bf89a97d18ed54ddfef54ba2f58e3067ddaa4d2c`.
  The refreshed eight-match verifier digest is `85775872b7a284f5`.
- Aggregate owner-executable evidence is now 14,889 captured frames. Fresh WSL
  Linux and native Windows builds pass 19/19 and 20/20 configured tests.
  M4 remains unfinished; broader shared-state and remaining exhaustive SSBM
  equivalence work stay active.

## 2026-08-05 normal-throw hitboxes and release semantics

- A new 894-row Falcon-on-Falcon Dolphin memory capture has SHA-256
  `368c623e49231aff0f70c8aa687345f10e615b121a675dbddcb8abd99a3a0b95`.
  `tools/verify_ssbm_falcon_throw.py` hash-pins the executable, full frame-data,
  and DAT sources and qualifies 181 throw-action frames. Forward, back, and up
  throw expose their exact three moving attack spheres on frames 11-17, 12-19,
  and 11-28; down throw correctly exposes none.
- The capture proves the ordinary hitbox first damages the captured fighter by
  5/5/4% with four synchronized hitlag frames while preserving the capture.
  The separate throw command then adds 4/4/3% on DAT-decoded frames 18/20/15
  with zero release hitlag. Down throw adds 7% on frame 20 with no preceding
  hitbox and zero release hitlag. Runtime now models both phases instead of
  applying only the release throw.
- Bystanders reuse the existing fixed-capacity generated sphere/effect path;
  captured victims use one capture-preserving scripted-effect path. Both share
  current-action stale scaling so simultaneous targets and the later throw
  command use the pre-move stale queue exactly once. No allocation, duplicate
  collision table, guessed timing, or hand-authored throw geometry was added.
- The canonical geometry digest is now
  `0a995d523bbeedf775559e7f53ea0558511188e8ef42e923e88a1d63e0d3e1b3`;
  the generated include SHA-256 is
  `c3305a58a7242e8b4fbceb54001ea622870cbb6110e313c8467d2ab3273296fe`,
  and the deterministic eight-match verifier digest is `940b60fe4d0b7a64`.
  Aggregate executable qualification is 15,070 frames. Fresh WSL Linux passes
  22/22 tests and native Windows passes 20/20. M4 remains unfinished; common-
  action hurt poses, source-Z collision semantics, exact sphere-versus-shield
  collision, and broader exhaustive SSBM equivalence remain active work.

## 2026-08-07 exact Falcon sphere-versus-shield collision

- A new 2,568-row Dolphin 3.4.0 memory capture has SHA-256
  `2df522e9bc93a09b61d15406f9281f4638f9c81796da349d033d90a112d51289`.
  It supplies 33 Falcon Jab 1 decisions at neutral, up-right, and down-right
  light-shield offsets. The observed last-hit/first-miss distances are
  28.60/28.65, 29.65/29.70, and 29.60/29.65 Melee units.
- `tools/verify_ssbm_shield_collision.py` hash-pins both that capture and
  `lbcollision.c` from decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7`. It reconstructs the live
  closest-point capsule distance and matrix-scaled radius sum; all 33 source
  predictions match the executable outcomes.
- Imported hit spheres and hurt capsules now share one correct Melee-root
  origin instead of being displaced by the simulation body's half height.
  Reference sphere/shield collision uses one squared radius-sum predicate in
  Melee's uniform spatial metric. The invalid rectangle/ellipse broad phase is
  skipped for reference spheres, removing both false negatives and redundant
  work while retaining the authored-rectangle path for custom content.
- Six deterministic boundary cases reproduce the three captured transitions
  with the observed controller pressure and guard axes. Aggregate executable
  qualification is now 17,638 frames. M4 remains unfinished; common-action
  hurt poses, source-Z semantics, and broader exhaustive SSBM equivalence stay
  active.
- The corrected root origin refreshes replay corpus/final SHA-256 to
  `47a9fe041eaf90013aa080907ca0168ca488616b95901f207cbe4cc755704590` and
  `370bdaa36efeeb6d0b7dc0278a46316018f68a8c0ace8c7a213327d142aea66f`;
  event-journal SHA-256 is
  `79bff77cc0438838f3c40ed054ac6d96396414deca781d0f3b03b07bfa637811`.
- Native Windows MinGW passes 20/20 configured tests and WSL Linux GCC passes
  22/22, including benchmark self-test and throughput smoke. The native replay
  verifier passes; WebAssembly replay verification remains delegated to the
  existing CI lane because the pinned Emscripten SDK is not installed locally.
## 2026-08-07 complete Falcon submotion-catalog import

- The raw owner `PlCa.dat`/`PlCaAJ.dat` and pinned DAT JSON now generate an
  exhaustive 318-slot Falcon submotion catalog: 275 decoded FigaTree
  animations and the 43 intentional empty slots. Each compact row retains the
  animation endpoint, last gameplay frame, action-script event count/offset,
  action flags, and source byte size. The catalog SHA-256 is
  `9bd124115e6eb66db0f6152dd6fade2886c85d1ae2368b4ae88b9084c7cc67ce`;
  the importer rejects incomplete counts, fractional/invalid frame endpoints,
  or changed pinned inputs.
- The same pass losslessly retains all 2,056 action-script events and 16,516
  encoded bytes. Exhaustive bounds/opcode/length tests and representative
  EscapeN/EscapeF source-byte assertions cover the O(1) accessor. The script
  digest is
  `6bf1021da93ea2f829c812b2bc425fe310808c8ad6e6e75eaf103c42b7ea4cfe`.
- All 275 FigaTrees are decoded rather than merely counted: 17,271 nodes,
  38,560 tracks, and 308,057 keys produce canonical SHA-256
  `d8a09bf451ce547d8d24634f40f654564e42cbf14ad5339a0c7d93ff7edc15dc`.
  Behavior-relevant tracks are emitted as compact tables on demand; unused
  visual animation keys are not linked into the hot runtime.
- Default dash, standing/run turn, run brake, landing, crouch, shield release,
  spot dodge, rolls, air dodge, tech, getup, and both taunt timings now query
  this single generated table. The exact default values and replay identity
  remain unchanged; 18 handwritten assignments were removed. State-specific
  endpoint/last-frame and entry-tick conventions remain explicit and are
  covered by the existing Dolphin-qualified transition tests.
- A 318-row typed command view decodes body-collision state-2/state-0 windows
  directly from the validated raw bytes. The default tech-in-place/roll 20-tick
  and both-orientation neutral-getup 23-tick windows now consume that table;
  their duplicate handwritten assignments are gone. Dodge and orientation-
  dependent getup/ledge mappings remain explicit qualification work.
- The generated include is 485,116 bytes with SHA-256
  `0507bbcb9dd25130b71c8e22b420e4c538033848405cb48222396e1c02ae7aec`.
  Native Windows MinGW passes all 15 configured tests and WSL GCC passes all
  22 configured tests. This closes the Falcon source-data inventory gap, not
  common callback, command, pose, ledge, or moving-collision behavioral gaps;
  those remain active M4 work.

- A reusable personal `ssbm-character-importer` Codex skill now captures the
  proven source hierarchy, no-guessing rules, DAT/FigaTree/action-script
  gotchas, zero-cost runtime pattern, four-column coverage ledger, and
  identical-input Dolphin workflow. Its generic inspector independently
  reconstructs Falcon's 318/275/43, 2,056/16,516, and
  17,271/38,560/308,057 totals and the same catalog/script/animation digests;
  skill validation and its pinned Falcon forward run pass.

## 2026-08-07 source-Z Falcon collision import

- The pinned Dolphin hurt-pose captures now generate both X/Y/Z endpoints for
  every imported 11-capsule Falcon pose. Runtime hit spheres retain their
  already-imported Z center, reflect X and Z with facing, and use one shared
  allocation-free Q16.16 3D point-to-capsule predicate. Shield sphere tests now
  include hit-sphere Z as well. Inspection schema 52 exposes the source Z
  center while the browser keeps its intentional X/Y projection.
- The canonical geometry SHA-256 is now
  `6a623a51717fc1c163b7b686c02f3dc336e2901ef3d85d501d4f76b037277fce`;
  the generated include SHA-256 is
  `4282ee1a77d2b8b204bb2d032a9982a63c9b1d7f55537d828bcbbc19adeaf9c5`.
  A source-derived down-tilt/standing-capsule case proves the former 2D false
  positive is rejected while the in-plane control still hits.
- The 20,000-tick combat trace exposed a valid synchronized throw state whose
  victim remains grabbed during ordinary throw hitlag. Snapshot validation now
  treats `HITLAG -> GRABBED` and `HITLAG -> THROW_*` as their effective linked
  actions, so hash/save validation accepts the same state the throw resolver
  already produces.
- Native Windows MinGW passes all 15 configured tests. WSL GCC passes all 22
  configured tests after refreshing the verifier soak digest for the changed
  content identity. The remaining collision gap is the executable's moving
  previous-to-current hit-capsule sweep, not source-Z data loss.

## 2026-08-07 source-routed Falcon defense callbacks

- The exhaustive Falcon animation import now emits one translation pool for
  all 65 translation-bearing submotions and 2,536 X/Y displayed-frame samples.
  The decoder uses `(flags & 0x3f) - 1`, which correctly resolves EscapeF's
  `0x800000c2`; a low-byte mask does not. Each of the 318 catalog rows exposes
  an O(1) span, and attack/root-motion accessors reuse it without duplicate
  generated arrays or runtime allocation.
- Forward and backward roll no longer use three authored velocity/displacement
  curves. Pinned decomp `ftCo_Escape_Phys -> ft_80085004 -> ft_80085030`
  proves that TransN offset replaces the ground-velocity channel. Both roll
  directions now consume the same source samples through a facing transform,
  including the exact 280-Q16 forward-roll frame-29 delta; the former additive
  backward displacement is removed.
- EscapeN/EscapeF/EscapeB/EscapeAir body-state commands now generate the
  production invulnerability windows with their state-specific displayed-
  frame bias. The comparator treats invulnerability as an exact discrete
  field. EscapeAir force, 0.25 dead zone, three-tick early item-throw window,
  and 0.9 decay come from `PlCo.dat`; entry-frame decay produces visible converted
  X/Y velocities 19,080/32,440 Q16.
- The raw EscapeAir opcode `0x4c` is decoded into variable index plus 24-bit
  value. Its displayed-frame-30 write of variable 0 to 1 switches the unchanged
  action from dual-axis decay to ordinary aerial input and gravity, matching
  `ftCo_EscapeAir_Phys`. A captured 48-frame EscapeAir ECB-bottom sequence
  replaces the generic extent that landed one tick early.
- The 285-frame Final Destination defense capture at SHA-256
  `1118a7a6e26ae98862e7457caee59ff45260076f30ce2e3e09ba71f249dc6084`
  passes end-to-end. It covers forward roll, spot dodge, backward roll, held-L/
  fresh-R upward air dodge, the ordinary-physics handoff, and landing with
  strict action/tick/grounded/facing/invulnerability/velocity checks and the
  established 640-Q16 position allowance. Aggregate executable qualification
  is now 17,923 frames.
- Pinned regeneration byte-matches at generated-include SHA-256
  `ffc2c3fb3ba2b2cb7591fb857b7396d6bc901a3e34a7cd5cb24c47334bfa86d3`;
  the expanded complete-source digest is
  `c7cf308115511ee2872fa9fe610f0bb264a7dde34f3f3edc357cbfe73822a015`.
  Replay corpus/final/event SHA-256 values are
  `af5b1bb66a475a4c28e93f15e12355d92c14ced6e08ecdad7bf25dbac82612f7`,
  `78f7eb6380ace1601da971dd021b90a60f53dd08d11a58ebdf930012b2ff0f12`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
  Content schema 70/fighter schema 63 hash and fail closed on the new callback
  boundary. WSL GCC, WSL ASan/UBSan, and native Windows MinGW each pass 15/15
  fidelity tests; the milestone combat verifier passes 61 registry rows, 74
  journal invariants, and the 20,000-tick deterministic trace. The repeated-
  match verifier is stable at
  `837cef55c577079e` after the fail-closed schema bump.
- The fresh WSL release benchmark self-test passes all ten available scenarios.
  A separate unsampled three-repetition throughput smoke reports 1,986,808
  representative-1v1 ticks/s, 1,083,197 representative-2v2 ticks/s, and
  1,084,402 maximum-combat-entity ticks/s. This short local smoke has no
  compatible history baseline and is not substituted for sampled profiling.
- The personal `ssbm-character-importer` skill gained reusable, repo-independent
  routines for validated action-script timelines, command-variable writes,
  body-state intervals, displayed-frame biases, six-bit translation-node
  selection, and caller-supplied Q16 translation sampling. Its inspector now
  verifies per-row translation nodes and Falcon's 65 translation submotions;
  skill validation, Python compilation, and the pinned Falcon inspection pass.
  The skill records the newly proven replacement/addition callback distinction,
  entry-frame decay ordering, same-action physics switches, animated-ECB
  landing, and Windows Slippi/Dolphin Memory Engine capture gotchas.
- M4 remains unfinished. This closes the sampled defense callback route, not
  common-action hurt poses, moving hit-capsule sweeps, remaining ledge/tech/
  damage semantics, or the exhaustive Falcon equivalence obligation.

## 2026-08-07 Falcon aerial-IASA executable-oracle slice

- The complete Falcon move table supplies fair/back-air/up-air/down-air IASA
  frames 36/29/30/38 and neutral air's source zero. The zero is treated as no
  IASA, not as an immediate interrupt.
- Pinned common aerial callbacks route an allowed interrupt through the shared
  double-jump primitive. One allocation-free generated predicate now accepts a
  displayed frame and serves grounded normals, special preprocessing, and
  aerials. One shared double-jump helper replaces duplicated wall-jump and
  ordinary-air transition code.
- Runtime output tick 0 is displayed frame 1, while the aerial interrupt
  callback evaluates the pending displayed frame. Primitive tests therefore
  exercise pre-step tick `IASA - 3` as the one-frame-early negative boundary
  and `IASA - 2` as the exact positive boundary for all four interruptible
  aerials. Nair receives a penultimate-frame negative control.
- Melee C-stick aerials now select the same directional Falcon scripts as the
  corresponding A-button direction. The route is guarded by exact imported
  Falcon timing/damage/landing-lag identity, so custom content continues to use
  its authored strong aerial.
- The 1,250-frame Final Destination capture at SHA-256
  `3a03c28fa78cf0c4de8fb7b4f4c873dee5df9ca44f46740b0f053da62cc2efaf`
  passes the at-will comparator for all 350 actionable jump, jumpsquat, aerial,
  and interrupt frames; settle/recovery rows are not counted as evidence.
  Aggregate executable qualification is now 18,273 frames. The affected
  deterministic eight-match verifier reproduces digest `8e6e4e326df3164f`
  twice after the new C-stick/IASA route.
- The `ssbm-character-importer` skill now records C-stick aerial aliasing,
  zero-IASA handling, and the independent state-entry/callback display biases.
  Its reusable `iasa_boundary` routine returns exact and adjacent-negative
  pre-step ticks; skill validation, Python compilation, and its Falcon-frame-36
  smoke check pass.
- Native Windows MinGW passes 15/15 fidelity tests; WSL release passes 22/22,
  and WSL ASan/UBSan passes 15/15. The rebuilt Emscripten playtest and headless
  Chrome/browser-adapter verifier pass, as do the focused movement/combat
  workflows and byte-identical native/Wasm 240-tick replay corpus.
- M4 remains unfinished. The sampled route closes double-jump aerial IASA, not
  the shared item throw/pickup and tether candidates, moving hit-capsule sweep,
  common-state hurt poses, or the exhaustive equivalence obligation.

## 2026-08-07 Falcon moving-hit capsule qualification

- Pinned decomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7`
  establishes that `ftHit.x58` is the prior transformed center and `x4C` is
  the current center. `lbColl_80006E58` intersects that moving hit capsule with
  the current hurt or shield capsule; its source file SHA-256 is
  `fa47d275f86956edb3c3a228a7fcc160e6f467c2d4bfd5f86d71f1d55e13e1fb`.
- The geometry importer now retains live collision state 2/3 independently
  from effect `groupId`, rejects new spheres whose previous/current centers
  differ, and requires continuing spheres to preserve the last temporal
  same-ID center. The last duplicate hitlag row supplies the next temporal
  predecessor while the first equivalent row remains canonical generated
  geometry. The canonical geometry digest is
  `d22af093cb93df154a9fd992294e50b2233d282d62f0b38aae9ef9d750b1a92b`;
  pinned regeneration byte-matches the tracked include at SHA-256
  `5e622abf609a32ec4d15eb2f0654f4f923e037d0a8df66b97874c5b935a27c55`.
- One shared C17 Q16.16 capsule-to-capsule closest-point predicate now
  serves attacks, grabs, and shields. It broad-phase rejects separated axes,
  specializes degenerate segments, and reduces the five dot products to the
  source float's 23-bit precision before the determinant so every intermediate
  remains portable `int64_t`. Runtime reconstructs continuation from the prior
  world state and same hitbox ID, adding no allocation or rollback bytes.
- A 274-frame Slippi Dolphin 3.5.1 capture at SHA-256
  `d8599ecc80efc567d579d9c3df9c10c70f89909dc38358ad29d602ca6ed3f4ea`
  uses Falcon down tilt frame 12 against Falcon grab. At 27.4 Melee units the
  current sphere misses by 0.451734762 units, the moving capsule overlaps by
  0.692950483, and the target takes 12%. At 28.3 units both predicates miss
  with -1.178471136/-0.182688971 margins and damage remains unchanged. The
  production Q16.16 integration reproduces the positive and negative decisions
  at will.
- The deterministic eight-match verifier is stable across repeated Windows
  and WSL runs at digest `766fb20cbc77ea08`, with unchanged 27 combat, two
  shield, eight KO, six projectile, eight rollback, and eight replay outcomes.
  Native and Wasm replay output remains byte-identical at corpus/final/event
  SHA-256 values
  `af5b1bb66a475a4c28e93f15e12355d92c14ced6e08ecdad7bf25dbac82612f7`,
  `78f7eb6380ace1601da971dd021b90a60f53dd08d11a58ebdf930012b2ff0f12`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
- The personal `ssbm-character-importer` skill now records executable hitbox
  lifetime, duplicate-row and post-hit sampling phases, rollback-free history,
  portable fixed-point constraints, and the required sweep-only positive plus
  nearby miss route. Its shared offline 3D segment-margin routine, Python
  compilation, collision smoke, and skill validation pass.
- A paired WSL Release diagnostic used the same three scenario binaries with
  15 repetitions and 50 ms samples. Current/b404807 median rates were
  1,197,499/1,023,615 representative-1v1 ticks/s,
  841,991/890,948 representative-2v2 ticks/s, and
  856,468/624,945 maximum-combat-entity ticks/s. The 2v2 delta is inside the
  run's high MAD, while the other two medians improve. The history database had
  no compatible baseline (`invalid_comparisons=10`), so this is diagnostic
  evidence only and not a regression or profiling qualification.
- Native Windows MinGW passes 20/20 configured tests; WSL Release passes 22/22
  and WSL ASan/UBSan passes 15/15. The rebuilt Emscripten playtest reports every
  startup probe passing, the in-app Browser starts a live match with a clean
  warning/error console, and the browser-adapter verifier passes. Native and
  Wasm replay corpus output is byte-identical. WSL's optional headless-Chrome
  helper was unavailable because Chromium is not installed; rendered Browser
  QA supplied the web check instead.
- M4 remains unfinished. This closes the executable moving-hit sweep, not
  remaining common-action hurt poses, aerial-IASA item/tether branches,
  ledge/tech/damage semantics, or the exhaustive Falcon equivalence obligation.

## 2026-08-07 Falcon Initial Dash and RunBrake hurt poses

- A 262-row Slippi Dolphin 3.5.1 capture at SHA-256
  `df7085d40479c81634a34796c830a4be73d81ab64cce10f218c5508d5f8a2958`
  supplies all 15 displayed Initial Dash poses and all 28 RunBrake poses. The
  verifier requires every frame to contain Falcon's 11 live capsules and
  compares port 2 against port 1 after facing-normalized Q16.16
  canonicalization. This cross-port check caught and prevented an early route
  whose metadata said Falcon while the menu predicate had actually selected
  Fox.
- The pinned positive route places Jab 1 against an inward Falcon Dash at 31.0
  Melee units: Dolphin deals 2%, and the reconstructed source collision margin
  is +0.289213242. The 31.5-unit control remains a miss at -0.156797621. The
  old generic rectangle misses the positive route by 3.503404617 units, so the
  pair specifically qualifies the animated common pose rather than merely the
  attack or damage path.
- Generation appends two compact common-pose tracks to the existing immutable
  frame table and reuses its deduplicated capsule pool. Runtime maps the public
  action once, uses the shared bounded span accessor, and adds no allocation,
  float math, duplicated capsule representation, snapshot bytes, or public
  action-index coupling. The canonical geometry digest is
  `32d599e74b4d5fe1f0a60e58f2ff6eb5efa2bae057c0e88b5fed55b72bfb24da`;
  pinned regeneration produces tracked include SHA-256
  `c985cf2098a2ca0876f8009d7c8ec7d997628b4347e067fc47d590b72fa170eb`.
- Shared offline `tools/ssbm_collision.py` now owns source-float segment
  margins, captured hit/hurt evaluation, Q16.16 pose canonicalization, and
  bounded pose equality. Both Falcon verifiers reuse it. The personal
  `ssbm-character-importer` skill records the menu/metadata drift hazard,
  cross-port proof, generic-rectangle discriminator, compact runtime pattern,
  and portable pose routines; Python compilation, its pose smoke, and skill
  validation pass.
- Native Windows passes 20/20 tests; WSL Release passes 22/22 and WSL
  ASan/UBSan passes 15/15. Repeated Windows and WSL verifier runs are stable at
  digest `c29d887a778f876d`. The rebuilt Emscripten playtest passes all startup
  probes, the browser adapter verifier passes, and native/Wasm replay output is
  byte-identical at corpus/final/event SHA-256 values
  `af5b1bb66a475a4c28e93f15e12355d92c14ced6e08ecdad7bf25dbac82612f7`,
  `78f7eb6380ace1601da971dd021b90a60f53dd08d11a58ebdf930012b2ff0f12`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
  The rendered in-app playtest starts a live four-stock match with all probes
  passing and no console warnings or errors.
- The prior moving-sweep CI exposed two stale verification pins rather than a
  target-specific behavior failure: the setup-contract script still expected
  the old 803-byte checkpoint and pre-sweep replay hashes, while Chrome still
  expected 84 instead of the current 83 typed replay events. The pins now match
  the already-qualified 807-byte checkpoint and native/Wasm replay identity;
  both exact failed workflow commands pass locally with WSL plus native Chrome.
- M4 remains unfinished. Common hurt poses beyond Initial Dash/RunBrake,
  aerial-IASA item/tether branches, and wider ledge/tech/damage behavior remain
  in the exhaustive Falcon equivalence obligation.

## 2026-08-08 Falcon CrouchStart and CrouchEnd hurt poses

- The common-pose oracle now contains 433 Slippi Dolphin 3.5.1 rows at SHA-256
  `aa64e6261e50130a70c6714e9b3177d44733a6c003de12cdff1581fb557380b0`.
  It retains the qualified Initial Dash and RunBrake tracks and adds every
  displayed CrouchStart frame 1-7 and CrouchEnd frame 1-10. Each frame exposes
  all 11 live Falcon capsules, and the CrouchStart frame-3 port-2 pose matches
  the independently captured port-1 pose after facing-normalized Q16.16
  canonicalization.
- The same-input discriminator places Jab 1 against CrouchStart frame 3.
  Dolphin hits at 17.7 Melee units with a reconstructed source-float margin of
  +0.131910442 and misses at 17.84 with -0.005252888. The old generic body
  rectangle falsely reports +0.596595764 at the miss distance, so the runtime
  negative case specifically exercises the imported crouch capsules. The
  verifier hash-pins `lbcollision.c`, `ftCo_Dash.c`, and `ftCo_Squat.c`.
- Generation appends two tracks to the existing compact common-pose index and
  reuses the same deduplicated capsule pool as every attack, special, Dash, and
  RunBrake pose. Public action values map once at the accessor boundary; there
  is no allocation, runtime float, duplicated capsule representation, or new
  snapshot state. The canonical geometry digest is
  `6d9946a838933cf47184f1d782dc84d0db1e384a8f1270a64aadc775a0d0c552`,
  and two pinned regenerations produce byte-identical include SHA-256
  `430dc17b4ac5ff57185ba6e740bca38abe64fd82c12e9d623e9b3c0687acba01`.
- The personal `ssbm-character-importer` skill now records a new source-backed
  guardrail: looping common animations cannot be flattened from a FigaTree
  endpoint. Falcon Run was observed at displayed frames `1, 2, 2` while its
  velocity settled, so it needs exact phase/rate reconstruction; bounded Dash,
  RunBrake, CrouchStart, and CrouchEnd tracks remain safe after their tick phase
  is proven. Skill validation and Python compilation pass.
- Native Windows passes 20/20 tests, WSL Release passes 22/22, and WSL
  ASan/UBSan passes 15/15. Repeated Windows and WSL verifier runs are stable at
  digest `31415c22fe408115`. Native and Wasm replay corpus output remains byte-
  identical; the rebuilt Emscripten client passes the exact Chrome smoke and
  browser-adapter verifier. The 13-scenario profiler workload passes, and a
  separate unsampled 64-environment run reports 1,738,207 single-world and
  1,622,754 batched ticks per second with exact state identity.
- M4 remains unfinished. Common hurt poses beyond the four qualified bounded
  tracks, aerial-IASA item/tether branches, and wider ledge/tech/damage behavior
  remain in the exhaustive Falcon equivalence obligation.

## 2026-08-08 ExiAI executable-oracle acceleration

- A prior-art sweep found Vlad Firoiu's maintained ExiAI Slippi/libmelee path,
  which already supplies a headless Linux AppImage, null video, EXI controller
  input, game-side fast-forward, a long-lived ENet helper, and instant match
  restart. The project now pins ExiAI 0.2.0's published AppImage at SHA-256
  `87e9ef6d80ed03354a1647d0616016dbc91399aa9e86a69ae5a398edd0a0c2bd`,
  `melee==0.47.2`, and `dolphin-memory-engine==1.3.1`. A WSL bootstrap verifies
  and extracts the ignored toolchain without touching the owner disc image.
- The rejected custom path was an exact Slippi 3.5.1 NoGUI/unthrottled build.
  WSL software OpenGL made its 601-row run take 38.15 seconds, so retaining a
  project fork would be both slower and duplicative. The binding plan now
  requires a prior-art sweep before any substantive implementation or tooling
  work and requires batched same-configuration experiment traces rather than
  one GUI launch per candidate.
- An unaccelerated/accelerated A/B using the same ExiAI executable, libmelee,
  memory observer, match, and 601-row Initial Dash/CrouchStart/CrouchEnd/
  KneeBend trace passes the automated acceleration verifier. All inputs, game
  frames, active states and action frames, movement, damage, hitlag, collision
  decisions, and active non-hitlag geometry are exact across 200 active Falcon
  and 194 active opponent rows, including 191 and 186 complete non-hitlag pose
  samples respectively.
- The verifier excludes only process-local addresses, menu-origin looping idle
  pose phase, and hurtbox endpoints while hitlag is positive. ExiAI skips
  display-side bone work in those frames; they are forbidden as imported
  geometry, while their already-completed collision decision and complete
  gameplay response stay strict. No other tolerance or field exclusion is
  permitted.
- The accelerated geometry-heavy route takes 15.02-15.27 seconds versus about
  28.3 seconds for the previous Windows stock path, a roughly 46% wall-time
  reduction with no GUI. The unaccelerated headless A/B control takes 17.01
  seconds because per-row memory probing dominates this trace. The full setup,
  commands, measurements, limitations, and upstream revisions are recorded in
  `docs/technology_decisions/ssbm_oracle_acceleration.md`.
- M4 remains unfinished. The accelerated oracle improves the evidence loop but
  does not itself close KneeBend, the remaining common hurt poses,
  aerial-IASA item/tether branches, or wider ledge/tech/damage behavior.

## 2026-08-08 Falcon KneeBend hurt poses

- The common-pose oracle now contains 650 ExiAI Slippi 3.5.1 rows at SHA-256
  `6169379625ff0f972d4bf4cc70b38cffedeb63a7dadea79b4973ee391eb1d1f1`.
  Its dedicated active, non-hitlag track captures all four displayed KneeBend
  frames with 11 live capsules each. KneeBend frame 2 canonicalizes identically
  across both controller ports. The pinned `ftCo_KneeBend.c` SHA-256 is
  `91249dcf7a0aa59277e8912bd8b5a82548262df66ef3426d6ed3d27cebdd6c12`.
- A same-input Jab 1 boundary hits KneeBend frame 2 at 16.5 Melee units with a
  reconstructed source margin of +0.153751175 and misses at 16.8 with
  -0.106885775. The old generic rectangle falsely reports +1.636596680 on the
  miss, so the negative route specifically qualifies the imported pose.
- A fresh 650-row unaccelerated/accelerated same-binary A/B passes all strict
  gameplay fields and 251 active non-hitlag Falcon pose rows. The control takes
  22.17 seconds and the selected headless/null/fast-forward run 16.68 seconds.
  The accelerated raw capture is accepted only because the imported KneeBend
  samples are active and non-hitlag; the verifier still forbids its idle and
  hitlag-only bone endpoints.
- Generation appends one compact four-frame track to the existing common index
  and reuses the shared deduplicated capsule pool. Runtime maps public
  `JUMP_SQUAT` once to KneeBend and adds no allocation, float math, capsule
  representation, snapshot bytes, or per-fighter state. The canonical geometry
  digest is
  `515b82d61819d8ab152b96e86595f9f2b18dae181ba1cdc053f6382c0d1782bb`;
  pinned regeneration produces tracked include SHA-256
  `0343852f516363f5f9f056d54728d2b7ff36d5e532fcbd07e21429a12a13af31`.
- Native Windows passes 20/20 tests, WSL Release passes 22/22, and WSL
  ASan/UBSan passes 15/15. The rebuilt Wasm target passes the browser-adapter
  verifier; native and Wasm replay output remains byte-identical at the prior
  corpus/final/event hashes. The verifier soak digest advances to
  `0d7e6b4d2930738f`, and a 64-environment unsampled run reports 1,762,812
  single-world and 1,779,525 batched ticks per second with exact state identity.
- M4 remains unfinished. Other common hurt poses, aerial-IASA item/tether
  branches, and wider ledge/tech/damage behavior remain in the exhaustive
  Falcon equivalence obligation.

## 2026-08-08 Falcon wall and ceiling response

- A prior-art sweep pinned `ftCo_PassiveWall.c`, `ftCo_PassiveCeil.c`,
  `ftCo_FlyReflect.c`, the common collision callbacks, Falcon's generated
  submotion/scripts, and the complete owner `PlCo.dat` before implementation.
  Production now imports the 1.0 collision threshold, 0.8 reflection scale,
  15-frame reflection invulnerability, three-frame re-collision lock,
  five-frame wall freeze, and 14-frame wall-tech invulnerability.
- A reusable runtime `MapCollData` reader validates Hyrule's 98 vertices and
  91 line records. Five checkpoint-isolated physical routes select line 70's
  right-facing pillar wall and line 47's cave ceiling, covering wall bounce,
  wall tech, wall-tech jump, ceiling bounce, and ceiling tech with held drift.
  The capture has 719 total rows and 145 focused response rows under semantic
  digest `5339134dd04cff9612e8c8a3e1d460f85018ae4c081ac7426fbad3cee3b785f5`.
  Three independent captures agree after explicitly normalizing only the
  directional damage-flight animation on the final pre-contact wall row.
- Production preserves reflection actions through hitstun, distinguishes wall
  hitstun clearing from ceiling hitstun preservation, and consumes Falcon's
  31/45/26 wall/wall-jump/ceiling durations. The comparator matches the
  five-frame wall freeze, 0.49/-0.13 wall release, 1.39/2.97 wall-jump release,
  0.06-per-tick ceiling drift, frame-11 1.99 release, and invulnerability
  boundaries within 0.0015 source units. Absolute position is excluded because
  source Hyrule and the compact production fixture are separate geometry
  domains.
- The new generic numeric domain contributes five cases and 60 stored samples.
  All four registered domains now run 32 cases plus replay in 0.404 seconds on
  WSL and 0.401 seconds on Windows. The accepted live pack measures 2.759
  seconds warm and 5.493 seconds end to end.
- Windows passes 21/21, WSL Release 23/23, and WSL ASan/UBSan 16/16. Browser
  adapter and collision-overlay gates pass. The intentional content-identity
  change yields verifier soak digest `453bfa2c1893f00d` identically across
  three Windows and three WSL runs.
- The `ssbm-character-importer` skill now records runtime stage-collision
  discovery, physical waypoint relocation, pre-contact trigger timing,
  passive-versus-reflection lifecycle separation, and semantic digest rules.
  It validates successfully. M4 remains unfinished; floor response, slopes,
  broader pushbox/collision combinations, remaining Falcon audit rows, and the
  native Battlefield frontend remain.

## 2026-08-08 Falcon common open-air damage response

- The prior-art/source sweep mapped pinned doldecomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7`, `ftCo_Damage.c`,
  `fighter.c`, the existing ExiAI checkpoint runner, and the owner
  `PlCo.dat`. A strict reusable HSD reader now validates the archive root and
  relocations, then imports all 518 words of `ftCommonData` instead of
  transcribing selected frame data. The owner archive SHA-256 is
  `63841336337eb5a7366b06ccc60ea4bd37c3604ab56e19939d78b9aa9cdd234c`.
- Production now uses separate self and knockback velocity channels. In open
  air it follows the source callback order: ordinary physics, imported 0.051
  knockback-magnitude decay in Melee coordinates, then integration of both
  channels. DI uses the source squared-projection formula and 18-degree cap;
  SDI/ASDI use the source radial 0.7 threshold, four-frame window, analog
  displacement, and C-stick ASDI priority. Vector operations happen before
  conversion through the project's different X/Y scales.
- A six-case checkpoint pack covers neutral, full-right DI, half-right DI,
  radial diagonal SDI, a below-radial control, and C-stick-priority ASDI. The
  live pack records 138 rows with stable observation SHA-256
  `51402cd3605ba2761e3c11ed6baab74eb1b7ab22136822507b39d0a00cc40d95`.
  It compares every selected hitlag/launch position, self velocity, knockback
  velocity, hitlag, and hitstun within 0.001 Melee units. Warm capture takes
  0.665 seconds; full live plus simulation lifecycle takes 3.438 seconds.
- The generic stored-oracle engine now supports allocation-free numeric trace
  domains, with no Falcon-specific case loop. The generated production trace
  SHA-256 is
  `dd946d72cc6348c502298cd92438d3617866cd5f8798e342b4057a1f200e812b`.
  Together `falcon-common-hurt` and `falcon-common-damage-response` cover 26
  registered cases and complete in 205.990 ms on Windows and 285.842 ms in
  WSL. Replay intentionally advances to corpus/final/event SHA-256 values
  `0100de6c59b7b31306710bfd55923fa78e367d996c4bd4d1a60dd6efd1db9c16`,
  `840e3df343bd58e176f80b48a2e05578537f326583ff10f29e162ff83eafaba0`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
- Native Windows passes 21/21 tests in 0.45 seconds, WSL Ubuntu passes 20/20
  in 1.94 seconds, and WSL ASan/UBSan passes 16/16. The deterministic
  repeated-match verifier digest is
  `07cc4f4247d83066`. The synthetic browser ladder fixture explicitly retains
  its authored minimal decay and therefore does not contaminate production
  Falcon's imported default. The personal `ssbm-character-importer` skill now
  records reusable HSD/common-data import, separate velocity channels, source
  callback order, and air-decay guidance and validates successfully.
- M4 remains unfinished. Ground knockback/friction and wall, ceiling, floor,
  tech, bounce, and getup response require their own live qualification before
  they may enter the stored oracle or be marked equivalent.

## 2026-08-08 Falcon flat-ground damage response

- The source sweep pins grounded damage classification, Sakurai-angle
  handling, `xF0_ground_kb_vel`, friction/projection order, damage levels, and
  the animation-plus-hitstun release boundary at doldecomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7`. The typed common-data import now
  exposes the required thresholds and ground constants from the same owner
  `PlCo.dat`; no response constants are guessed.
- Production owns separate self, `x8c` knockback, and ground-tangent `xF0`
  channels. `DamageLw1/2/3` use imported submotion durations, ordinary control
  cannot overwrite them during hitstun, and a ground-to-air edge conversion
  retains projected `x8c` while clearing stale `xF0`.
- One checkpoint-isolated late-DashAttack route records 64 rows and selects 15
  damage samples. Live and simulation agree on effective action/frame,
  grounded/tumble, damage, hitlag, hitstun, self velocity, projected
  knockback, and `xF0` within 0.001 source units. Observation SHA-256 is
  `e08d7149e3f46d814d5c4a709e316cf3063208bb9673141effe6b1958f03fc79`;
  position is explicitly reserved for the separate pushbox domain. Warm work
  takes 0.128 seconds and the full live lifecycle 2.801 seconds.
- The generic numeric stored oracle now carries action, resume action,
  grounded/tumble, damage, action tick, and ground-knockback fields. Its new
  domain production digest is
  `954931140122b77790e334b4d1742709c853fcc060fdc692fc9e45522ff7a379`.
  All three domains cover 27 cases and include deterministic replay in 0.305
  seconds on Windows.
- Mid-damage canonical save/load is exact without expanding the 807-byte flat-
  stage format: load reconstructs `xF0` from the serialized flat-tangent
  `x8c`; state schema 61/save format 57 make the three new damage actions and
  their resume/release semantics fail closed. Slopes remain an explicit future
  versioned-serialization boundary.
  Windows release passes 21/21 in 1.13 seconds, WSL release 23/23 in 1.32
  seconds, and WSL ASan/UBSan 16/16 in 10.99 seconds. Native/Wasm replay is
  byte-identical, the standalone kernel/browser verifiers pass, and the full
  headless Chrome smoke passes with the repinned 81-event replay.
  Replay identities intentionally advance to corpus/final/event SHA-256
  `6531cd69c3f0766ffb5c252ec0e4799b0a4ff5353ce1a7aa31ae37d740a28046`,
  `0cd7a7327a0e6fdbdaf149ebd12f69c473446a5d1a85e017c4b5df51cb68b16f`,
  and `509d826181cd7d047a2241b06fda4cb4c875477bd5b0828fcafcb65865b80ae5`;
  the repeated-match verifier digest is `d3fdaa3dc317d4b4`.
- The reusable importer skill now records the distinct scalar/channel model,
  source operation order, flat-snapshot reconstruction limit, hitlag resume
  action, and damage release rule. M4 remains unfinished: slopes, pushbox-
  coupled position, wall/ceiling/floor collision, tech, bounce, getup, and
  wider damage routes still require their own live domains.

## 2026-08-08 executable-oracle checkpoint pack

- A fresh prior-art sweep covered upstream Dolphin batch/null/NoGUI support,
  Dolphin Memory Engine, libmelee, and ExiAI/Slippi rollback support. The
  selected base remains ExiAI revision
  `bf1aec4de4856eab412996137287f447daa8ae17`; its existing
  `SlippiSavestate` avoids disconnecting the live ENet observer.
- `tools/ssbm_exiai_checkpoint.patch` added a small atomic file-control channel
  to the pinned NoGUI runner. At this historical checkpoint, loads occurred
  while Melee was blocked at the EXI input boundary and immediately rebased
  the restored checkpoint. The 2026-08-11 common-special slice supersedes that
  policy with immutable multi-slot protocol v2 after a divergent branch proved
  that immediate rebasing was not valid isolation. The reproducible WSL build
  remains `tools/bootstrap_ssbm_checkpoint_oracle.sh`.
- The DME reader now resolves a fighter with two contiguous reads, snapshots
  each complete fighter once, and reads each unique bone matrix once. Linux
  ExiAI capture exposes the same interface over Dolphin's read-only MEM1
  shared mapping, avoiding per-field `process_vm_readv` calls. A
  329-row comparison against the accepted defense trace is exact after the
  already-documented idle-pose normalization.
- The common-hurt pack is reduced from 4,198 rows/26 repeated cases to 283
  serialized rows/eight checkpoint-isolated cases. It retains all 255 imported common
  action poses plus one live Dash collision hit/miss integration pair. The
  remaining per-pose boundaries use the same exhaustive capsules and pinned
  decomp collision routine offline instead of duplicating long Dolphin routes.
- Two independent runs have identical canonical pose SHA-256
  `3a1b182dc64ee6db6caa7cc316c633e3330a9001344ca88f5cd57a441b48cdf1`
  and identical Dash margins `+0.289212401/-0.156798480`. Against the accepted
  4,198-row artifact, every pose is Q16.16-equivalent; only 30 components in
  24 poses differ, each by exactly one Q16.16 least-significant bit. Setup
  ticks still execute, but the observer skips undeclared actions; redundant
  shield dwell, terminal holds, and recovery commands are removed. Five fully
  verified warm pack times are 2.635-2.729 seconds, down from 37.6 seconds and
  below the manifest-owned three-second changed-domain budget. Lifecycle
  timing isolated a further nine-second repeated cost to hashing the unchanged
  1.4-GB disc. An atomic stat-keyed digest cache preserves the pinned digest,
  invalidates on any path/size/mtime/ctime change, and reduces unchanged full
  capture lifecycle time to 7.27-8.13 seconds. The patched NoGUI runner polls
  checkpoint requests every 5 ms; all eight acknowledgements take about 0.075
  seconds. A cross-process Slippi reconnect prototype was rejected after its
  event stream desynchronized and blocked, so persistence remains one live
  connection per packed invocation.
- The reusable `ssbm-character-importer` skill now records the checkpoint,
  rebasing, coalesced-read, canonical-digest, and non-duplicated discriminator
  rules and validates successfully. M4 remains unfinished.

## 2026-08-08 manifest-selected stored equivalence lane

- The ordinary post-build edit loop no longer needs Dolphin for the registered
  Falcon common-hurt domain. One generic registry selects affected domains,
  validates generated metadata, runs the filtered production oracle, checks
  declared counts and source/production digests, and runs the pinned replay
  corpus. An unrelated documentation change correctly selects no domain.
- The character-independent Python selector/generator and shared C runner own
  all selection, schema, hashing, source-frame mapping, case dispatch, failure
  reporting, replay, and budget logic. Falcon-specific action identifiers,
  complete pose spans, 20 controls, and three thin production adapters live in
  its domain manifest/combat test. The generated include contains no capsule
  copy; a thin adapter reads the production accessor, and static assertions
  guard manifest completeness.
- All 255 production-accessed poses hash to
  `33e7ceea1447113256972a719f3abc981857d6a0cd67432842100b74dc50a613`;
  the live source payload remains pinned at
  `3a1b182dc64ee6db6caa7cc316c633e3330a9001344ca88f5cd57a441b48cdf1`.
  The manifest registry/domain aggregate is identical on Windows and WSL at
  `270f7e71a30500401ac97c18ced42e341f89a75443b5482bfaca343d5c642326`.
- Five warm complete runs take 116.845-120.355 ms on native Windows MSVC
  Release and 148.121-166.786 ms on WSL GCC 13.3 Release, far below the
  two-second plan target. Focused combat/stored-oracle CTest takes 0.41/0.44
  seconds respectively, and the full combat executable passes on both. The
  complete configured matrices pass 23/23 on native Windows and 16/16 in the
  isolated WSL equivalence build.
- This closes both the no-Dolphin edit-loop and warm changed-domain live
  budgets for the registered domain, not the full equivalence infrastructure:
  only one domain is registered, and uncovered Falcon behaviors in the
  fidelity audit remain active work.

## 2026-08-08 Falcon ordinary Landing hurt poses

- The mandatory prior-art sweep checked the existing common-pose importer and
  runtime lookup, the reusable `ssbm-character-importer` skill, the accepted
  common-state Dolphin capture, maintained public import/extractor work, and
  pinned doldecomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7`
  before implementation. Common motion state 42 resolves to submotion 15;
  its source endpoint is 30, gameplay endpoint is 29, and it has no TransN
  stream. With no interrupt input, the executable exposes displayed Landing
  frames 1-30 even though Falcon's separate landing-lag gate opens after frame
  4. Animation duration and interruptibility are not conflated.
- The expanded common-pose oracle contains 4,198 rows. The accepted accelerated
  SHA-256 is
  `8ddb3245936d9ded82763481010e67f5968dbe7b50d14fe251db4ae25fedfbcc`;
  the same ExiAI binary without fast-forward produces control SHA-256
  `32a0a742012f360c1e49b27d2fb2023e16eac5af23694b032a3777d41ad16a9d`.
  Strict A/B comparison passes all rows, 2,011/1,149 active fighter/opponent
  rows, and 1,909/1,120 initialized non-hitlag action-owned pose rows. The
  accelerated/control captures took 37.6/45.3 seconds locally; those timings
  preserve oracle validity but are explicitly not acceptable iteration
  throughput, so capture-pipeline optimization precedes the next long route.
- Jab 1 frame 3 evaluates ordinary Landing's pending source-frame-22 pose. It
  hits at 20.3 Melee units and misses at 20.6, with reconstructed margins
  +0.142592999/-0.150664469. The old generic rectangle misses the positive
  route at -1.863413048, so action labels or the fallback body cannot satisfy
  the control.
- The importer appends one compact 30-frame entry to the existing common index,
  reuses the deduplicated capsule pool, and maps public `LANDING` once. It adds
  no allocation, float math, capsule representation, snapshot bytes, or per-
  fighter state. Canonical geometry SHA-256 is
  `377fb771847ff7a6a3dcb6c02e648787c279fc68a5c3eee9aab16ce23d5fe645`;
  two pinned regenerations are byte-identical at tracked-include SHA-256
  `b39310bffe4ac6ec61e4711481e28f24dd01811dcdfdded7d52c921ac8ad415e`.
- Windows MSVC and WSL Release pass 22/22 tests; WSL ASan/UBSan and Windows
  MinGW pass 15/15. The standalone combat/browser verifiers and Chrome smoke
  pass with emergent-technique-specific tests skipped. Rebuilt native/Wasm
  replay output is byte-identical at corpus/final/event SHA-256 values
  `5893af587684844c22c0fc6c7019f13748c4366c586e088ed1a24d4e1819c942`,
  `0235c47f05fdd37257bdd59ac5cfd5c7e107316a19f99001eefdeea7d78e951d`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
  The deterministic verifier digest advances to `c408c172ee853571`.
  Live-browser QA reports every probe passing, `controllers 2/2 · GameCube
  4/4`, advancing ticks, and no warnings/errors. The 13-scenario profile
  workload passes with ten available scenarios; a separate unsampled 64-
  environment boundary reports 232,384 single-call and 1,348,050 batched ticks
  per second, a 5.8010x speedup with exact state identity.
- The reusable importer skill now records that a full no-input animation span
  must be qualified independently from an earlier interrupt gate, and validates
  successfully. Aggregate owner-executable evidence is 18,697 qualified
  frames. M4 remains unfinished; the next slice first optimizes the oracle's
  cross-process capture path before further fidelity expansion.

## 2026-08-08 Falcon horizontal LandingFallSpecial physics

- The mandatory prior-art sweep checked the existing movement implementation,
  defense comparator/capture route, imported common submotion catalog, reusable
  `ssbm-character-importer` skill, maintained public import/extractor work, and
  pinned decomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7` before
  implementation. No maintained upstream importer supplied a more complete
  runtime route. The pinned decomp establishes
  `ftCo_Landing_Phys -> ft_80084F3C -> ftCommon_ApplyGroundMovement`, while
  common LandingFallSpecial submotion 36 contains no TransN stream.
- The previous 285-frame defense capture entered LandingFallSpecial only with
  zero horizontal velocity and therefore could not qualify the friction
  callback. The expanded 329-frame route adds a natural down-left air dodge
  that lands above Falcon's walk maximum, crosses the source's high-speed and
  ordinary-friction branches, and observes all ten landing ticks. Accelerated
  SHA-256 is
  `d9dfebcb6e42f5e71ece08490429b61083f81bee067def379b5fdd6270d96b95`;
  same-binary unaccelerated control SHA-256 is
  `d78abcfe3d252d0f87409aba3343cd838efb739d6311494d520f2f076eb5255f`.
  A/B comparison passes all 329 rows, 225 active-fighter rows, and 185
  qualified active-pose rows.
- Production no longer invents a ten-tick +/-2,051-Q16 animation displacement
  or stores its sign in `dash_direction`. Entry copies incoming horizontal
  speed into the existing ground-velocity channel; friction is 0.16 Melee
  units per tick while speed exceeds walk maximum and 0.08 afterward. On the
  flat captured stage every position delta is the post-friction velocity. The
  removed shim first diverged at capture row 310/source frame 4 by 1,889 Q16
  position units even though velocity already matched.
- The source comparator passes all 329 rows with only the established 640-Q16
  representation envelope. Windows MSVC and WSL Release pass 22/22, WSL
  ASan/UBSan and Windows MinGW pass 15/15, and combat/browser verifiers pass
  while emergent-technique-specific tests stay skipped. Native/Wasm replay
  output is byte-identical at corpus/final/event SHA-256 values
  `5893af587684844c22c0fc6c7019f13748c4366c586e088ed1a24d4e1819c942`,
  `0235c47f05fdd37257bdd59ac5cfd5c7e107316a19f99001eefdeea7d78e951d`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
  Live-browser smoke reports the new final replay hash, all probes pass, the
  PC-mode adapter exposes `controllers 2/2 · GameCube 4/4`, and browser logs
  are empty. The 13-scenario profile workload passes. An unsampled 64-
  environment boundary reports 255,418 single-call and 1,529,695 batched ticks
  per second, a 5.9890x speedup with exact state identity. The deterministic
  verifier agrees across Windows and WSL at digest `b1417182d96ecd2d`.
  The canonical invariant that position delta equals post-friction velocity
  adds no duplicate state or per-tick abstraction. M4 remains unfinished.

## 2026-08-08 Falcon FallSpecial/LandingFallSpecial hurt poses

- The mandatory prior-art sweep inspected pinned doldecomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7`, libmelee, the Melee frame-data
  extractor, the complete common submotion catalog, and the existing shared
  pose importer. No maintained upstream tool supplied a complete production-
  ready common-hurt importer. Common motion state 35 resolves exactly to
  submotion 26; LandingFallSpecial state 43 resolves to submotion 36 rather
  than a similarly named character-local motion. Pinned SHA-256 values are
  `19217b0e24dc138f601b4c9914975da0879ece0a71ef968272fac75238aad6f4`
  for `ftCo_FallSpecial.c` and
  `7e33d64809df680df293eeec1189299ab0f77d633f39c00dcd6756faab7d08e8`
  for `ftCo_Landing.c`.
- The final 3,818-row oracle imports FallSpecial's full displayed frame-1-8
  loop and LandingFallSpecial's speed-scaled source-frame sequence
  `1, 4, 7, 10, 13, 16, 19, 22, 25, 28`, each with all 11 capsules and zero
  source damage. Its accelerated SHA-256 is
  `ccfbb5edaa952760a4058a98213ee3fb6b54cd3bd9900e8c25aaa2535e4c8a5e`;
  the exact same ExiAI binary without fast-forward produces control SHA-256
  `8f27b27e82cbcb3f50db6595f050580c921ad7316a40f1b819489c73df12da1c`.
  Their strict A/B comparison passes 1,773/1,049 active fighter/opponent rows
  and 1,674/1,023 initialized, non-hitlag, action-owned pose rows.
- A pending FallSpecial frame-5 control hits at 15.5 Melee units and misses at
  16.2, with reconstructed margins +0.563753525/-0.130038736; the generic
  rectangle falsely hits the miss at +2.936591339. A pending
  LandingFallSpecial source-frame-7 control hits at 18.5 and misses at 19.3,
  with margins +0.509528504/-0.137582566; the generic rectangle falsely misses
  the hit at -0.063411903. These are physical geometry discriminators, not
  action-name or damage-only assertions.
- The importer appends two compact tracks to the existing deduplicated capsule
  pool. A single inlined action-specific source-frame adapter maps states that
  enter at source frame 1 separately from zero-based public action timers. It
  adds no allocation, float math, duplicated capsule representation, snapshot
  bytes, or per-fighter state. Canonical geometry SHA-256 is
  `181c614a07192564a18c779515c53e7f906c34a1d6ececf67e912cb8ed8d2e42`;
  byte-identical regeneration produces tracked include SHA-256
  `f9e823f41da8ae4684791ba1aa667f903e422bc23d237139634118230607e6df`.
- Native Windows passes 22/22 tests, MinGW passes 15/15, WSL Release passes
  22/22, and WSL ASan/UBSan passes 15/15. Combat/browser verifiers, rebuilt
  Wasm native/Wasm replay identity, the 13-scenario Tracy-instrumented
  workload, and live in-app-browser QA all pass. Reset restarts the served
  match from tick 740 to tick 15 with no console warnings or errors. Replay
  corpus/final/event SHA-256 values remain
  `af5b1bb66a475a4c28e93f15e12355d92c14ced6e08ecdad7bf25dbac82612f7`,
  `78f7eb6380ace1601da971dd021b90a60f53dd08d11a58ebdf930012b2ff0f12`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
  The deterministic verifier digest advances to `152f4a132141f83c`.
  Separately, the unsampled 64-environment boundary reports 315,095 single-
  call and 1,675,603 batched ticks per second, a 5.3178x speedup with exact
  state identity.
- The reusable `ssbm-character-importer` skill now records Linux process-name
  truncation, same-binary oracle qualification, exact motion/submotion
  resolution, looping versus non-looping frame counts, speed-scaled source-
  frame sequences, and action-specific runtime pose adapters. It validates
  successfully. M4 remains unfinished: additional common hurt poses,
  aerial-IASA item/tether branches, and wider ledge/tech/damage behavior remain
  in the exhaustive Falcon-equivalence obligation.

## 2026-08-08 Falcon AirDodge hurt poses

- The required prior-art sweep reused pinned `ftCo_EscapeAir.c`, the generated
  FigaTree/body-state and EscapeAir attribute tables, the existing 48-frame
  defense/ECB trace, libmelee action mapping, common-pose importer/collision
  helpers, and the qualified ExiAI runner. No maintained upstream importer
  supplied the missing complete action-owned hurt-pose track. The pinned
  callback SHA-256 is
  `cdff68de39d55855f1ca02b8e4af09ce856a1133cc21b23921a881b23e0dfaf6`.
- The final oracle contains 3,004 rows. The accepted accelerated capture is
  `e0eb4279e1ce19690cebf57142f20342fdb42ee1bfe78cfa84d702fc4c705055`;
  its matched unaccelerated control is
  `64c5d69c495bd37fbc60e335affc9f578afd9219a39dadd08ab65eaa7207ba41`.
  A/B comparison passes 1,430/849 active fighter/opponent rows and 1,337/829
  initialized, non-hitlag, action-owned pose rows. The native-jump source
  route captures exactly AirDodge frames 1-49 with 11 capsules each, unchanged
  damage, and FallSpecial afterward. Frames 1-3 and 30-49 are vulnerable;
  frames 4-29 are invulnerable.
- A low offstage control prevents floor contact while keeping a grounded Jab
  in reach. Jab 1 hits AirDodge frame 31 at +21.0 Melee units and a +3.0-unit
  root height, then misses at +21.8. Reconstructed margins are
  +0.391159288/-0.406612492, while the generic rectangle falsely misses the
  positive route at -2.563399506. Both controls relocate over safe stage before
  ledge/death state can ignore placement and contaminate the next experiment.
- The importer appends one compact 49-frame entry to the existing common index,
  reuses the single deduplicated capsule pool, and maps public `AIR_DODGE`
  once. It adds no runtime allocation, float math, capsule representation,
  snapshot bytes, or per-fighter state. Canonical geometry SHA-256 is
  `70d25e01c090cc0a2deb12fcf8ea3e7c8c847840f03638a0aa045285ad6eae99`;
  byte-identical scratch/tracked regeneration produces include SHA-256
  `dfadbc2bdc54d601099e95e6340310132b54b13ddcd99cba5f5885724c0cfce2`.
- Native Windows passes 20/20 tests, MinGW passes 15/15, WSL Release passes
  22/22, and WSL ASan/UBSan passes 15/15. Combat/browser verifiers, live Chrome
  smoke, the 13-scenario profiler workload, and native/Wasm replay identity all
  pass with emergent-technique probes skipped. The replay corpus/final/event
  hashes remain
  `af5b1bb66a475a4c28e93f15e12355d92c14ced6e08ecdad7bf25dbac82612f7`,
  `78f7eb6380ace1601da971dd021b90a60f53dd08d11a58ebdf930012b2ff0f12`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
  The deterministic verifier digest advances to `c8907b160c578a53`; an
  unsampled 64-environment run reports 368,931 single-call and 1,652,858
  batched ticks per second with a 4.4801 boundary speedup.
- The reusable `ssbm-character-importer` skill now records complete aerial-pose
  altitude selection, magnifying-glass damage avoidance, low offstage
  discriminators, and post-observation safe relocation. M4 remains unfinished:
  additional common hurt poses, aerial-IASA item/tether branches, and wider
  ledge/tech/damage behavior remain in the exhaustive Falcon-equivalence
  obligation.

## 2026-08-08 Falcon RollForward/RollBackward hurt poses

- The prior-art sweep reused pinned decomp `ftCo_Escape.c` at SHA-256
  `762d18265d193e9d4b0b701a7a8048bb8824a4de5f505ceef00e316c1e56fb89`,
  the generated submotion/body-state and TransN tables, the existing 285-frame
  defense capture, the shared common-pose importer/collision helpers, libmelee
  action mapping, and the qualified ExiAI runner. No maintained upstream tool
  supplied a more complete roll-pose import path. The older defense evidence
  established that the two animations remain distinct after facing
  canonicalization: each has 31 unique displayed poses.
- The finalized common-pose oracle contains 2,427 rows. Its accepted ExiAI
  capture is
  `0bdf1390f8dbee759f58f520c4f30dc2b12c6d793d8aab01eed0b3abf26caf93`;
  the same-binary stock control is
  `2df3834629235db4dfc5a12f71af0fac034786aed1e34f8bdc6156c592c2299c`.
  A/B comparison passes 1,100/721 active fighter/opponent rows and 1,010/704
  qualified action-owned pose rows. Every source and collision route now
  pre-places both ports safely, settles, establishes explicit facing through
  controller input, fully recovers, and only then applies final placement.
  This makes the trace independent of prior damage-facing updates, residual
  velocity, edge position, and airborne state.
- Both executable and generated state commands report roll frames 1-3
  vulnerable, 4-19 invulnerable, and 20-31 vulnerable. Jab 1 hits the
  facing-left RollForward frame-22 pose at 12.98 Melee units and misses at
  14.18; reconstructed margins are +0.792811248/-0.129717661 and the generic
  rectangle falsely reports +3.515399933 on the miss. Jab 1 hits facing-right
  RollBackward frame 24 at 20.00 and misses at 20.75; margins are
  +0.332847599/-0.293575032 and the generic rectangle falsely reports
  -1.558547020 on the positive route.
- The importer appends two compact 31-frame entries, reuses the one
  deduplicated capsule pool, and maps public `ROLL_FORWARD`/`ROLL_BACKWARD`
  once. It adds no allocation, float math, new capsule representation,
  snapshot bytes, or per-fighter state. Canonical geometry SHA-256 is
  `4939ad7ab5ea7c446be5427c6f91aa45a4802f62083e20ca62fcdff27dd40063`;
  byte-identical scratch/tracked regeneration produces include SHA-256
  `80106a687936b75e9e2ebfc33d289468ddec8fffebb60959dc167a6165070475`.
- Native Windows passes 20/20 tests, WSL Release passes 22/22, and WSL
  ASan/UBSan passes 15/15. Combat, browser-adapter, and live Chrome smoke
  verifiers pass while emergent-technique probes remain skipped. Native/Wasm
  replay output is byte-identical at corpus/final/event SHA-256 values
  `af5b1bb66a475a4c28e93f15e12355d92c14ced6e08ecdad7bf25dbac82612f7`,
  `78f7eb6380ace1601da971dd021b90a60f53dd08d11a58ebdf930012b2ff0f12`,
  and `deef8e9aa4b32bac5cb4597f8383f91056fc1b0e7d98d34d0e71202e7dea675b`.
  The verifier soak digest is `9a538aa99be7742a`; a 64-environment
  unsampled run reports 1,783,497 single-world and 1,811,566 batched ticks per
  second with exact state identity.
- The reusable `ssbm-character-importer` skill now records the safe
  pre-placement/settle/facing/recovery routine discovered by this slice and
  validates successfully. M4 remains unfinished: further common hurt poses,
  aerial-IASA item/tether branches, and wider ledge/tech/damage behavior remain
  in the exhaustive Falcon-equivalence obligation.

## 2026-08-08 Falcon SpotDodge hurt poses

- The prior-art/source sweep reused the pinned common Escape implementation,
  generated submotion/body-state catalog, existing defense trace, shared
  common-pose importer, and ExiAI runner. The pinned `ftCo_Escape.c` SHA-256 is
  `762d18265d193e9d4b0b701a7a8048bb8824a4de5f505ceef00e316c1e56fb89`.
  The executable and generated command table agree that SpotDodge frames 1-2
  are vulnerable, 3-20 are invulnerable, and 21-32 are vulnerable.
- The expanded common-pose oracle contains 1,099 rows. The unaccelerated
  same-binary control is
  `bb75f231b80b3c6397b02355277bf621071a7f6ae2f5a85f3558d27d0b25bfc7`;
  the accepted ExiAI capture is
  `dbd01434760f87236d2569b64fbe6bb7d77f6723d7d61322a48c94eab5f0089a`.
  Their A/B compares 482/295 non-standing fighter/opponent rows and exactly
  matches 446/284 initialized, non-hitlag, action-owned pose rows. The
  qualifier now also excludes `GuardReflect` endpoints because the decomp
  enters it with `Ft_MF_SkipAnim`; gameplay state and collision outcomes stay
  strict.
- Both collision controls reset both ports to facing right and zero velocity,
  preventing the positive hit's victim-facing update from mirroring the
  negative route. Jab 1 hits the pending SpotDodge frame-24 pose at 21.0 Melee
  units with reconstructed margin +0.264534944 and misses at 22.0 with
  -0.272110224. The generic rectangle reports -3.563406944 on the positive
  route, so the hit specifically discriminates the imported capsule pose. The
  verifier models the post-frame observer's displayed-pose/collision-report
  ordering explicitly rather than relabeling the observed frame.
- Generation appends one 32-frame entry to the existing compact common index,
  reuses the shared deduplicated capsule pool, and maps public `SPOT_DODGE`
  once. It adds no runtime allocation, float math, duplicated capsule format,
  snapshot bytes, or per-fighter state. The canonical geometry digest is
  `f2124b2cd0068006a13f29cad0e45bf5148dd9b8dc83a1be16332576717ebced`;
  byte-identical pinned regeneration produces tracked include SHA-256
  `b81cdeb333382d091b4f01764cb855148d7b436df3cd84e0cfa6eff489ac4dfb`.
- Native Windows passes 20/20 tests, WSL Release passes 22/22, and WSL
  ASan/UBSan passes 15/15. The standalone combat and browser verifiers pass
  while explicitly skipping emergent-technique tests. Wasm rebuilds cleanly;
  native/Wasm replay output remains byte-identical at the existing
  corpus/final/event hashes. The deterministic verifier soak digest is
  `9e249f8c1116e7e9`. A 64-environment unsampled run reports 1,743,765
  single-world and 1,758,518 batched ticks per second with exact state identity.
- M4 remains unfinished. Other common hurt poses, aerial-IASA item/tether
  branches, and wider ledge/tech/damage behavior remain in the exhaustive
  Falcon equivalence obligation.

## 2026-08-08 Falcon flat-floor impact and tech response

- Prior-art and pinned-decomp review established directional-tech,
  neutral-tech, then missed-tech landing
  priority. The common-data importer now owns the 20-frame tech window,
  40-frame lockout, 0.2 direction threshold, and 220-frame DownWait value;
  Falcon's generated submotions and `TransN` tracks own the 26/26/40/40
  response lifecycles and directional root motion. Production preserves the
  incoming self/knockback channels and hitstun memory on the callback frame,
  projects/decays ground knockback on the following tick, and snapshot
  validation distinguishes retained source memory from active stun.
- The new `falcon-common-floor-response` domain captures four Final Destination
  cases over 804 rows / 232 focused observations. Three independent captures
  share semantic SHA-256
  `85fd93638bcb26b8b6e405cb1008a396acf05d132e07c7f9dcc3b6993034dd3f`.
  Forty-eight production samples agree on action/tick, invulnerability,
  preserved hitstun memory, and forward/backward imported root translation
  within 0.0015 source units. Position is explicitly deferred to the stage and
  pushbox domain; DownBound's source ECB-grounded toggles remain a named gap.
- The generic stored gate now covers five domains / 36 cases plus replay in
  0.564 seconds on WSL and 0.543 seconds on Windows. WSL release passes 24/24
  in 1.45 seconds, native Windows MinGW release passes 17/17 in 1.05 seconds,
  focused WSL ASan/UBSan passes 4/4, and the browser adapter passes. The
  deterministic verifier digest is now
  `2f8ea9f2d6d1bd78`. M4 remains unfinished: DownBound ECB poses, DownWait/getup
  choices, slopes, ledge departure, pushboxes, and the remaining fidelity-audit
  rows are next.

## 2026-08-08 Falcon Down-orientation prone/getup response

- A fresh prior-art and pinned-source sweep covered `ftCo_DownBound.c`,
  `ftCo_Down.c`, `ftCo_DownAttack.c`, `ftCo_DownStand.c`, the common input
  helpers, current doldecomp head, and libmelee. The decomp callbacks remain
  unchanged from the pinned revision, and no upstream route replaces the
  existing checkpointed ExiAI oracle. `PlCo.dat` now supplies the 0.2 stick
  threshold, 50-degree horizontal wedge, 60-frame A/B buffer, 0.6625 upward
  C-stick threshold, and 220-frame DownWait timeout. Falcon's imported
  submotions and body-state/`TransN` tracks supply every response duration,
  invulnerability boundary, and roll displacement.
- The manifest-driven capture path now supports action/frame-conditioned input
  edges and segmented observation windows without adding another
  character-specific transport. Ten checkpoint-isolated cases cover timeout,
  buffered A/B, upward C-stick, forward C-stick roll, backward main-stick roll,
  upward-stick and shield neutral getup, attack-over-roll priority, and a
  threshold-adjacent C-stick negative. Two independent 2,370-row captures have
  the same semantic SHA-256
  `fc91d42660ac0a8df8f0715b183b2ec97bccfe2ee0279491cadf915e64044438`.
- Production routes raw A/B edges into the common-state buffer before
  projectile/item/special adapters consume effective inputs. C-stick edge
  history shares the existing directional byte, while the combined A/B age
  adds four canonical bytes. Getup rolls consume the imported orientation- and
  direction-specific root track each frame; no authored roll-speed duplicate
  remains. The save is now 811 bytes (`PFSAVE52`), state schema 62, and save
  format 58.
- One hundred twenty sparse production samples agree with both live captures
  on action/frame, option priority, invulnerability, roll direction, prone
  orientation, and root velocity within 0.0015 source units. Position is
  explicitly assigned to the stage/pushbox domain; DownBound's 4-grounded,
  18-airborne, 4-grounded ECB sequence and the opposite prone orientation
  remain named gaps rather than inferred successes.
- The generic stored lane now covers six domains / 46 cases plus replay in
  0.465 seconds on WSL and 0.628 seconds on native Windows. WSL Release passes
  25/25 in 0.92 seconds, native Windows MinGW passes 18/18 in 0.75 seconds,
  focused WSL ASan/UBSan passes 5/5 in 6.80 seconds, and the browser adapter
  passes. Replay corpus/final/event digests are
  `7c0a7c7a332e95e34fe414436c7d0c9d34faafc460264e4488dc83c66f0f820d`,
  `466a56f8b8767534b22ba11e8c61643c68f5b559cba9006b2095fb2259fc9745`,
  and `509d826181cd7d047a2241b06fda4cb4c875477bd5b0828fcafcb65865b80ae5`.
  The reusable importer skill now records the conditional-edge, raw-input,
  compact-buffer, sparse-comparison, and live-object-override rules learned by
  this slice. M4 remains unfinished.

## 2026-08-09 Falcon paired-player-push registered equivalence

- A fresh pinned-decomp and upstream sweep confirmed that the relevant
  `ftcommon.c`/fighter player-push route is unchanged at current doldecomp head
  and that Slippi/libmelee provide observation transport rather than an
  existing reusable equivalence domain. The production implementation was
  retained; this slice promotes its earlier 540-frame live proof into the
  character-independent manifest, generator, selector, stored runner, replay,
  and budget architecture.
- The live checkpoint pack covers both controller ports and push directions,
  compares both Falcon lanes, and observes Melee's fixed 0.3-unit grounded
  nudge. Three fresh ExiAI boots produce the same 48-row / 96-lane source trace
  SHA-256
  `3c6ade86d516474c60b7559690b3b858f2b7a66b41982859e4f81df70a7c73f5`;
  production produces
  `079a34868db4fff30719d7a784d7bd102aab7a81acd189ad6293c65a9056bc7a`.
  Action, facing, and grounded state are strict, horizontal velocity permits 32
  Q16 units, and relative position permits 2,692 Q16 units for the documented
  conversion plus one source nudge.
- The generic numeric trace runner now supports one or two manifest-declared
  lanes and an exact serialized-field mask. Existing one-lane domains retain
  their original mask and all five established production digests. Repeated
  input phases expand offline into immutable arrays with compile-time lane
  counts, so production gains no allocation, parsing, host floats, duplicated
  paired comparator, or character switch.
- The complete seven-domain gate covers 52 cases plus deterministic replay in
  0.802 seconds on Windows and 0.716 seconds in WSL. Windows and WSL Release
  both pass 26/26 tests; WSL ASan/UBSan passes 19/19. The reusable character
  importer skill now records the paired-lane, field-mask, flattened-input, and
  two-port qualification rules. M4 remains unfinished: slopes, broader
  ECB/stage topology, remaining fidelity-audit rows, and the native Battlefield
  frontend are still open.

## 2026-08-09 imported Hyrule slope and ordinary-ledge equivalence

- A prior-art sweep of pinned/current `doldecomp/melee`, ExiAI, Slippi, and
  libmelee found observation transport but no maintained importer or exact
  stage-response equivalence domain. The accepted importer converts Hyrule
  MapCollData lines 34-37 from source joint-local coordinates to immutable
  runtime world-space lines. Its semantic source digest is
  `4a0dd57bb8d9532589d3ecd129213d3a0876538a2dc7f733eca6c1e73c04db9c`.
- Falcon's generated frame data now includes the 24-frame DamageFly ECB-bottom
  track and ledge-snap attributes. Production projects landing attack
  knockback onto the imported slope tangent, preserves source landing-entry
  ordering, evaluates DownBound contact before decay, leaves support only at
  the exact endpoint boundary, clears hitstun on collision-driven Fall, and
  checks ordinary ledges using source root/ECB coordinates.
- Two checkpoint-isolated cases qualify line-34 forward getup roll and the
  natural line-36-to-line-37 departure through exact ordinary ledge catch. Two
  fresh Dolphin boots reproduce source digest
  `8c62ce678732b38d157f1e3cee2409b0da22835bc63c906c41e765ce1a879a6d`;
  production digest is
  `4dce7db6baa8a11fb90b77438ef5e423b500e5e5a3f7a4da835bfc979d6f0167`.
  The final live gate took 1.234 seconds warm / 4.859 seconds end to end.
- The generic numeric oracle supports per-case serialized-field masks without
  runtime dispatch. Explicit inherited-zero initializers keep GCC `-Werror`
  and MSVC portable. The corrected DamageFly clock/grounding and landing order
  were also requalified in the existing damage and floor packs.
- The complete stored lane now has eight domains / 54 cases plus replay and
  takes 0.804 seconds on Windows and 0.778 seconds in WSL. Windows and WSL
  Release pass 27/27; WSL ASan/UBSan passes 20/20; the verifier soak is stable
  across both hosts at `7f584c16f3d23773`. Current replay is 41,599 bytes with
  corpus/final/event digests
  `0d1c16c1e231d29c89a49d193f6b10deb081297821d5448239307cae4d33f4ad`,
  `6c648e4463b070ad4b7e3b013ea620e21463b281fe39b00980cf0cbf558bfcd5`,
  and `0cf114479e7cec86ebe0b89b08fd6eabc74209d99ed053fb92b397d26d6eab8e`.
  M4 remains unfinished: ordinary Fall animation phase, distinct
  EdgeCatch/EdgeWait pose/root behavior, broader stage topology, remaining
  fidelity-audit rows, and the native Battlefield frontend are open.

## 2026-08-09 Falcon CliffCatch to CliffWait equivalence

- The prior-art sweep compared pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` with current upstream
  `013091add6d46d2d809d163371deab97ab5e37eb`. `ftcliffcommon.c`,
  `ftCo_CliffWait.c`, `ftCo_Fall.c`, and `ftanim.c` are unchanged, and no
  maintained Slippi/libmelee route replaces the existing checkpoint oracle.
  The already-qualified Hyrule trace contains the needed rows, so this
  extension required no redundant Dolphin boot.
- Pinned source enters `CliffCatch` submotion 216 for seven displayed frames,
  then `CliffWait` 217. Its physics positions from absolute
  `x68C_transNPos` plus the selected endpoint rather than consuming only a root
  delta. The importer now emits live-qualified frame-one catch/wait anchors and
  reuses the existing decoded catch `TransN` deltas. Production exposes a
  distinct public `LEDGE_CATCH` action, converts source root to body center
  exactly once, ignores catch inputs, and enters wait only on animation
  completion.
- The expanded live theorem covers 180 selected rows / 110 production samples.
  It strictly checks the seven catch frames, first wait frame, endpoint/facing,
  invulnerability, action clock, zero velocity, and bounded Q16 position. Its
  source semantic SHA-256 is
  `0b23132b7a217ff173397faf8ac9e59169092c99095b4b4e3fbd885526b7a3f3`;
  production is
  `9c562426f42c4b01b08a7bbea9c667f56661a2787d107870a14208f326ccd94e`.
  Snapshot/hash validation and exclusive ledge ownership now include catch,
  and the ledge-invulnerability regression correctly counts the seven elapsed
  catch frames instead of restarting protection at wait.
- The generic eight-domain / 54-case stored gate passes in 0.605 seconds on
  WSL and 0.805 seconds on native Windows MSVC. WSL Release passes 27/27 in
  1.00 seconds, Windows MSVC Release passes 27/27, and WSL ASan/UBSan passes
  20/20 in 8.55 seconds. The Emscripten client rebuilds; browser-adapter,
  native/Wasm replay identity, and Windows Chrome DOM/Wasm smoke pass. Two
  standalone validation scripts now link the source-derived stage-data module,
  and the browser smoke pins the current canonical 75-event replay rather than
  its stale pre-repin count.
- The reusable importer skill now records the absolute-root-versus-delta rule,
  body-center conversion, full catch lifecycle, and snapshot/ownership gotcha.
  M4 remains unfinished: ordinary Fall animation phase, later wait/ledge-option
  behavior and geometry, broader stage topology, remaining fidelity-audit rows,
  and the native Battlefield frontend are open.

## 2026-08-09 Falcon ordinary airborne submotion clock

- A prior-art sweep compared pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` with current upstream
  `013091add6d46d2d809d163371deab97ab5e37eb`. `ftCo_Fall.c`, `ftanim.c`,
  and `fighter.c` are unchanged, and no maintained source-equivalent runtime
  could be reused. Falcon's owner DAT supplies JumpF 35, JumpB 50,
  JumpAerialF 50, JumpAerialB 35, and six eight-frame Fall-family motions.
- The common-data generator now validates and emits `ftCommonData.x78`, the
  exact 0.125 backward-jump selection threshold (axis 4096). Runtime preserves
  one 16-bit source submotion per fixed fighter slot while continuing to expose
  one allocation-free public `AIRBORNE` action. JumpF/JumpB transition to Fall,
  JumpAerialF/JumpAerialB transition to FallAerial, and Fall families wrap at
  the imported animation length. The shared action clock is the displayed
  source frame minus one.
- The existing Hyrule live theorem now compares the complete ordinary Fall
  sequence `1..8,1..5`, rather than excluding its clock. It passes 180 selected
  rows / 110 production samples with unchanged source semantic SHA-256
  `0b23132b7a217ff173397faf8ac9e59169092c99095b4b4e3fbd885526b7a3f3`
  and production SHA-256
  `918a548c6de3a53ab04f89d8bc5232e97b298d7887dc67661f42bd02a9626d66`.
  The independent 1,250-frame Dolphin aerial-IASA capture also passes all 350
  JumpAerial/FallAerial action-frame comparisons.
- Canonical state schema 64/save format 60 (`PFSAVE54`) appends eight bytes of
  source submotion identity: payload 695 bytes, checkpoint 835 bytes, replay
  41,607 bytes. Replay corpus/final/event digests are
  `02f52e1f9c9dbf29e21264c50d2139b8968c6bff810da3b30e00d9ba34fb2e0b`,
  `e5c235be9bf70b79f62d383b2bbc58db55ffa602c9ce513adbc8a7df3ac0c257`,
  and `0cf114479e7cec86ebe0b89b08fd6eabc74209d99ed053fb92b397d26d6eab8e`.
  WSL Release passes 27/27 tests in 0.49 seconds, Windows MSVC Release passes
  27/27 in 6.14 seconds, and WSL ASan/UBSan passes 20/20 in 6.15 seconds. The
  full stored gate passes in 0.781 seconds on WSL and 0.821 seconds on Windows.
  Native and Wasm replay output is byte-identical; browser-adapter and Windows
  Chrome DOM/Wasm smoke validation pass.
- The reusable importer skill now records how to retain source submotions and
  imported directional predicates behind a coarser public action. M4 remains
  unfinished: later ledge options, broader stage topology, remaining audit
  rows, and the native Battlefield frontend are still open.

## 2026-08-09 Battlefield wall and ceiling runtime routing

- The prior-art sweep compared pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` with current upstream
  `ee4311b81417eb49b77ac73f3e9dc41c6a1e3dee`. The relevant `mpcoll.c`,
  `mplib.c`, `ftCo_Fall.c`, and `ftcommon.c` paths are unchanged. Current
  libmelee supplies observation/projection helpers, but its platform projection
  explicitly omits ECB changes and cannot replace the executable oracle.
- Reference-stage production no longer treats Battlefield's central underside
  as one authored solid rectangle. Two allocation-free fixed-point queries walk
  the imported ceiling/right-wall/left-wall ranges, validate line kind and
  endpoints, select the first crossed source surface, and return one resolved
  body position. Ordinary movement, hitlag/SDI rejection, and stationary wall
  probing reuse these queries; authored-stage behavior remains unchanged.
- Focused primitive coverage exercises all five ceiling and twelve wall lines,
  wrong-direction and outside-geometry controls, and representative production
  collisions against the central underside ceiling and wall. Strict Windows
  MSVC `/W4 /WX` focused combat and movement suites pass.
- The source floor query sweeps previous/current ECB bottom; it does not accept
  an arbitrary already-below point. The compact ordinary-airborne adapter now
  shares one overflow-safe delayed-crossing predicate across authored and
  imported pass-through platforms. Its compatibility branch is bounded to one
  immediately preceding crossing, preserving the saved Battlefield frame-95
  `Landing` transition while a reference-stage below-platform negative remains
  airborne.
- This intentional production correction changes the seeded verifier soak.
  Three Windows and three WSL executions independently reproduce digest
  `70dda9b2e5d9d936`, eight stock-result matches, 2,847 ticks, 37 combat events,
  eight KOs, eight rollbacks, and eight replay verifications; the reviewed
  digest is repinned.
- Fresh post-repin validation passes Windows Release 28/28 in 1.74 seconds,
  WSL Release 28/28 in 1.95 seconds, WSL ASan/UBSan 21/21 in 13.50 seconds,
  and all nine stored domains / 73 cases plus replay in 1.129 seconds on
  Windows and 1.013 seconds in WSL. The importer skill package validates,
  native smoke passes,
  and the rebuilt interactive client was relaunched.
- This is deliberately recorded as partial. The current production body sweep
  uses the compact rectangle rather than Falcon's complete action-specific ECB,
  and does not yet consume the source-selected line normal for reflection or
  response. Identical-input live Battlefield contact qualification remains
  before this slice can be promoted.

## 2026-08-09 Battlefield landing entry and source Pass qualification

- The decomp route is `ftCo_8009A228` -> `ftCo_Pass_Anim` / `_Phys` /
  `_Coll`, with `ft_80084DB0` for common air physics and `ft_80082F28` for
  ground/ledge collision. Falcon raw `Pass` submotion 209 has a 30-frame animation;
  common-data x470 is a separate nine-frame floor-skip countdown. The runtime
  keeps both identities without a new allocation, public action, byte, or
  schema: `AIRBORNE` carries source submotion 209 and its zero-based phase,
  while the existing collision field owns the skip timer.
- A focused headless/null/unlimited ExiAI route entered Pass normally, then
  relocated Falcon after entry to prevent natural landing from truncating the
  pose. It captured all frames 0..29 in 5.2 seconds. Raw capture SHA-256 is
  `0dc57f8ffb85549be76b3b5a0017690b0df16905456169eaceaa2e7975eedc0c`;
  the canonical big-endian `(u32 action frame, f32 ECB bottom Y)` stream is
  `90060e614f359189c32b25d76b780b3fa92861dfdcfae0fd357dcc07ec10e6f8`.
  The generator emits the complete immutable Q16.16 table and regenerates
  byte-identically from the pinned owner extract.
- `ftCo_Landing_Enter` reaches `ftCommon_8007D7FC`, which switches ground state
  without clearing incoming self Y. Production now retains that value on the
  Landing entry row and projects it to zero on the following grounded-physics
  update. The complete saved 348-frame Battlefield identical-input capture
  passes on Windows and WSL within the established 640-Q16 position envelope;
  its former frame-276 divergence is closed.
- Complete-source identity is now
  `147520a32bd20dc99dc2f326f52f8fcfc56c57058cf99669c762eea0c776720a`.
  Because replay serialization includes content identity, corpus SHA-256 is
  now `02f52e1f9c9dbf29e21264c50d2139b8968c6bff810da3b30e00d9ba34fb2e0b`;
  final/event digests remain unchanged. Three Windows plus three WSL seeded
  soaks reproduce `3238a603d9ab2a5b` with identical 8-match / 2,847-tick
  outcomes before the pin is updated.
- Fresh validation passes Windows Release 28/28 in 3.35 seconds, WSL Release
  28/28 in 1.92 seconds, WSL ASan/UBSan 21/21 in 13.55 seconds, and the full
  nine-domain / 73-case stored gate plus replay in 0.925 seconds Windows and
  0.904 seconds WSL. Manifest SHA-256 is
  `f16fa189ecb621a54c6ed4921aa920a257316b0fbeb869fb21caa08104ccefb3`.
- The reusable importer skill now records the action-clock versus collision-
  timer distinction and the post-entry relocation method for complete ECB
  capture. M4 remains unfinished: selected-line/normal response and the wider
  Falcon fidelity audit continue.

## 2026-08-09 production Battlefield support and JumpF ECB qualification

- The earlier 348-frame movement runner reproduced Battlefield's vertical
  layout with authored primitives. It proved action/position timing but did
  not exercise the shipped `reference_collision_profile`. The runner now uses
  `pf_m4_reference_stage_content(BATTLEFIELD)`, reaches the oracle's left
  platform through ordinary walk/jump inputs, and emits the selected support
  in its allocation-free CSV trace.
- The comparator reads the source floor index from the existing collision-
  memory probe and requires `native support == source line + 1` on every
  grounded row. The platform phase selects source line 2/support 3; the final
  Pass landing selects main-floor line 1/support 2. The complete route passes
  all 348 frames on Windows and WSL within the established 640-Q16 position
  envelope.
- This stronger route exposed a real frame-95 landing gap. The simulator used
  Falcon's steady-state Fall ECB during JumpF, while the source animation has
  35 distinct displayed poses. A focused headless/null/unlimited ExiAI capture
  entered JumpF normally and relocated only after action entry, preserving all
  35 frames without a landing truncation. Raw capture SHA-256 is
  `28c4e902d8860f6d02ec779004c67c7ab94f87c7f3970699cfd9a44a8844cf1d`;
  canonical big-endian `(u32 displayed frame, f32 bottom Y)` SHA-256 is
  `6db927d319942e07d90ba6dd30aad39ad40bb42ab3cc09d498ea2587bfe233bb`.
- The generator emits the complete immutable Q16.16 JumpF ECB table and
  runtime selects it through the already-canonical source submotion. The
  bounded one-update pass-through compatibility path now applies only to
  approximate poses; exact imported ECB schedules use the ordinary sweep.
  Landing enters on source route frame 95 and retains incoming Y in inspection
  before the next grounded-physics update projects it to zero.
- Complete Falcon source identity is now
  `46c97fcbe303628fb1bf0ce3415431c01c16a0e73961ce5a1e78dd5dd1f1bfa9`.
  Three Windows and three WSL seeded verifier runs reproduce digest
  `3c3f20d38cee6e59` with identical eight-match / 2,847-tick outcomes. Replay
  corpus/final/event identities remain unchanged.
- Windows and WSL Release each pass 28/28 after the intentional verifier pin
  in 1.91 and 1.78 seconds; WSL ASan/UBSan passes 21/21 in 13.66 seconds. The
  nine-domain / 73-case stored gate plus replay passes in 1.168 seconds on
  Windows and 1.008 seconds in WSL. Generator regeneration is byte-identical,
  and the updated importer skill validates. Selected floor identity and
  absolute vertical resolution are now qualified for this route. Selected-
  normal response and live Battlefield wall/ceiling routes remain open.

## 2026-08-09 exact Battlefield sloped wall and ceiling response

- A prior-art/source sweep pinned decomp commit
  `9509dc04406fb2028bfab01243841ba4787c0fb7`: `mpLineGetNormal` derives the
  unit normal as normalized `(-dy, dx)`, `lbVector_Mirror` subtracts twice the
  normal projection, and `ftCo_800C18A8` reflects combined self/damage motion,
  applies common multiplier `x1BC`, stores damage velocity, and clears self
  velocity. libmelee 0.47.2 explicitly omits ECB evolution in its projection
  helper, so it remains useful prior art rather than source truth.
- The stage importer now preserves each line's source-space unit normal before
  the project's anisotropic coordinate transform. Wall/ceiling response shares
  one allocation-free Q16.16 mirror routine and transforms the result once.
- Three independent Falcon captures expose the same complete 24-frame
  DamageFlyN top/side ECB schedule under semantic SHA-256
  `9efade94dbd61446decfabeedce910e4a2823bfc65299b7ecb4cb31fb368eee1`.
  The generator pins and emits top, bottom, side-X, and side-Y tables; complete
  Falcon source identity is now
  `0adc405c5affe87ae3bcc84e7665b53869231e0f4ffa6f4043586bd953782df3`.
- Production sweeps the previous/current ECB side point against candidate wall
  segments and chooses the earliest intersection. It resolves the root to the
  contact fraction, gives wall response priority over a nearby ceiling, applies
  the frame's gravity before reflecting total velocity, and leaves reflected
  displacement for the following tick. This fixed both a line-15 contact that
  incorrectly selected ceiling line 10 and a 5.6% horizontal-response error.
- The new checkpoint-isolated Battlefield theorem covers ceiling line 10 and
  right-wall line 15, including their live selected normals, then compares 24
  post-response samples across action/state, action time/displayed frame,
  grounded/tumble/invulnerability, hitstun, both velocity channels, and relative
  position. Source digest is
  `8a0c463ffae10b1567815013c85c500bcb25869727874086c96d0e9c522a2f68`;
  production digest is
  `107ea657a7bad069ea8ee02cb98306dd116b78838c8e6899a4adf9ff6fcf0982`.
  A second fresh boot reproduces the source digest in 0.367 seconds warm and
  3.096 seconds end to end.
- The generic stored lane now registers ten domains / 75 cases plus replay and
  passes in 1.031 seconds on Windows and 1.048 seconds in WSL. Full validation
  passes Windows Release 29/29 in 1.70 seconds, WSL Release 29/29 in 1.89
  seconds, WSL ASan/UBSan 22/22 in 13.86 seconds, and the saved 348-frame
  production Battlefield route on both hosts within 640 Q16 units. Three
  Windows and three WSL seeded soaks reproduce `d3b4c23cb8a9dd7e` with
  identical 8-match / 2,847-tick results before the verifier pin changes.
- The importer skill now records full side-point ECB intersection,
  source-normal transformation, and gravity/reflection ordering. M4 remains
  unfinished because the broader fidelity-audit rows and hands-on frontend
  qualification are still open.

## 2026-08-09 reflected-action ECB and Battlefield floor re-contact

- A prior-art/source review confirmed that Melee keeps the real reflection
  action clock running after Falcon's ceiling-bounce presentation clamps at
  displayed frame eight. The runtime therefore retains one action clock and
  clamps only the shared pose lookup; no duplicate state or pose track was
  introduced.
- The checkpoint protocol now supports synchronized post-entry fighter and
  collision-position reset plus explicit self/knockback velocity overrides.
  One 0.739-second warm pack captures all nine observable ceiling poses and 51
  wall poses while preserving the native wall/ceiling response entry.
- A reusable extractor canonicalizes both captures facing-right, validates
  repeated displayed frames at Q16.16 identity, and emits one compact full-ECB
  profile. Raw/profile/semantic identities are
  `f1989a139185635d41d5cc2a51b0f88d41c1a26cf24c57fa82614feed6fda1c2`,
  `d6ccb5701f0bada0d7de1874004281e8ca46fcc0070db94e529d84d3fc637608`,
  and `9d162fe7917f0c23894ad1fe54a1a665d5c8e446d5ca439180811d706b2431a5`.
- Production consumes those poses through the same zero-cost adapter as
  DamageFly. Facing is applied once at the world-wall boundary. A source-root
  to simulator-body-center conversion fixed the initial apparent 1-2-frame
  landing lead without changing physics or tuning a threshold.
- The new live theorem follows 111 focused samples through native Battlefield
  line-3 re-contact: ceiling lands on sample 57 and wall on sample 54. Source
  and production trace identities are
  `4e9a0ad3222bd0d6b6d7ab7def0177cf4b5c361bded3826abfe2e91f9210dd5a`
  and `222a5504d62bc5500e57a88a0adad108b931ea73d2b70cdf46faccde3f36d2db`.
- The generic stored registry now covers 11 domains / 77 cases plus replay.
  Three warm Windows runs pass in 1.123-1.136 seconds and WSL in 1.121 seconds.
  Full Windows/WSL combat and replay checks, deterministic regeneration, and
  focused WSL ASan/UBSan pass. Fresh Windows and WSL native-client smoke builds
  also pass; the Windows build uses a separate output because the hands-on
  playtest executable remains open. M4 remains open for the remaining fidelity-
  audit rows and the hands-on native controller gate.

## 2026-08-09 exact quick/slow ledge hurt poses

- ExiAI fast-forward was rejected for this geometry family: repeated projected
  captures preserved action/state/root behavior but skipped display-bone work
  and produced different hurt endpoints. The accepted path keeps checkpointed
  headless/null/unlimited input while disabling fast-forward and reads all
  eleven collision-authoritative hurt capsules from one fighter snapshot.
- Two independent compact runs produced byte-identical 450-row, eight-case
  captures at SHA-256
  `3055455eb02949e15c240f563a49648578b6c5affa4dc5dd7ca62f2c7b19c1e3`.
  Ten quick/slow climb, roll, attack, and two-phase jump tracks contain 434
  complete displayed poses under semantic SHA-256
  `9125200e3e162822131fd8805ae1551371c4ebf0abc2256bba9a167cc181103a`.
- A character-independent profile extractor and manifest-driven C generator
  emit 4,774 immutable capsules. Production retains the already-serialized
  source submotion and selects the exact track through the shared allocation-
  free pose lookup used by combat and collision inspection; legacy common-pose
  callers keep their zero-submotion adapter.
- Strict Windows and WSL builds pass, as do focused movement, combat, and
  deterministic replay tests on both hosts. Both hosts independently reproduce
  the intentional new seeded match-soak identity `52600d79f2b95349`, replacing
  the pre-ledge-geometry golden.
- The generic stored domain now consumes the hash-pinned ledge profile directly
  and derives all ten spans, so their frame metadata is not duplicated. A
  compact source-submotion key distinguishes tracks that share one public
  action. The combined live source digest covers 689 poses at
  `2aadf4b37b26796bdbc08fe026b234542f2c61914a4488e35e0dccd72a72e151`;
  production independently serializes the same accessor surface at
  `d691705692841bfabb8a2407ab31037bf398b097fc461574ecd07954e16a4331`.
- The complete eleven-domain / 77-case gate plus replay passes in 1.12-1.14
  seconds on Windows and 1.15-1.29 seconds in WSL. The capture verifier compares
  only the physical capture contract, so adding stored projections cannot invalidate
  unchanged live evidence or conceal capture-plan drift. The smallest physical
  ledge hit/miss discriminator remains before this geometry family is complete.
  Broad Release validation passes Windows GCC 28/28, WSL GCC 23/23, and strict
  portable MSVC 30/30; WSL ASan/UBSan passes 23/23.

## 2026-08-09 live quick-climb collision discriminator

- The mandatory prior-art/source sweep reused the existing checkpoint sharder,
  source-pinned `lbcollision.c` capsule predicate, common-hurt verifier, and
  no-fast-forward ledge pose profile. No second emulator layer or copied pose
  table was introduced.
- Checkpoint manifests now distinguish default cases from supplemental
  projection cases. The pure projection helper is shared by capture and
  verification, and ordered conditional edges let one route schedule both
  `CliffWait` frame 2 input and quick-climb frame 25 opponent attack from
  semantic state rather than a guessed wall-clock delay.
- Two fresh two-shard captures are byte-identical at SHA-256
  `f31de47e694e46bf2269945747c97238ce443ddf88cbadc0a8e4214026f2785d`.
  Their canonical semantic SHA-256 is
  `fbe0cf877402bf82aba10d8ae3dceecb4e431caa87d9b75b5844bfb7b132af2d`.
- The miss row proves the target is imported `ledge_climb_quick` frame 29 and
  the port-2 attacker exposes the exact three-sphere Falcon Jab 1 frame-4
  signature. The positive placement deals two damage and enters
  `DAMAGE_NEUTRAL_2`; moving the attacker 0.75 Melee units farther leaves the
  target untouched. Reconstructed margins are `+0.573037244` / `-0.098777672`;
  the generic rectangle remains negative at `-1.895534515`.
- `tools/verify_ssbm_falcon_ledge_collision.sh` regenerates and verifies the
  theorem at will. The accepted run takes 6.067 seconds warm, inside its
  eight-second guardrail. The default 19-case ledge pack and ordinary stored
  edit loop are unchanged because supplemental routes are selected only when
  requested.

## 2026-08-10 common ledge down-input boundary

- A fresh prior-art sweep compared pinned and current `doldecomp/melee` plus
  existing libmelee/Slippi/Dolphin workflows. No current decomp change touches
  the governing `ftcliffcommon.c`, `mpcoll.c`, or `PlCo.dat` layout, and no
  maintained executable implementation supersedes the existing oracle.
- The common-data importer now extracts `x480` as an immutable ledge-grab
  down-axis threshold. Production consumes the generated Q15 value in the
  shared ordinary-ledge predicate; there is no runtime float, parser, or
  character switch.
- Two adjacent quantized controller routes reuse the existing Hyrule endpoint
  fixture. Observed source Y `-0.65` catches, while `-0.6625000238` rejects.
  The accepted route also exposed and fixed same-frame knockback clearing on
  `CliffCatch` entry.
- The imported DownBound contact mask now drives endpoint callback ordering:
  its first contactless frame consumes the current root, and subsequent
  contactless frames may retain the preceding floor root. This aligns both the
  original neutral departure and the new down-input route without duplicating
  action setup.
- The pack grows from two cases / 180 rows to four cases / 290 rows and 220
  stored samples. Two warm Dolphin runs take 2.716 and 2.493 seconds and share
  source semantic SHA-256
  `9df8c72fca21359281d7d89391a9c363e08e6cf5c06db8873868e10521f27b49`;
  reviewed production SHA-256 is
  `73f3dae4bf726aedd1e2ab37911818faa9b3fff4d1a19ed2a92a41148f142f5d`.
- The live comparator models Q16 position accumulation as a bounded per-tick
  consequence of the already capped velocity conversion error. State and
  transition fields remain strict. Three Windows and three WSL seeded soaks
  reproduce the intentional new identity `52e8cab76719e97c`, with identical
  8-match / 2,848-tick event totals. Windows GCC Release passes 28/28, WSL
  Release 30/30, WSL ASan/UBSan 23/23, and the eleven-domain / 79-case stored
  registry plus replay passes in 1.277/1.370 seconds on Windows/WSL. The local
  pinned MSVC workflow cannot start because Visual Studio `vswhere.exe` is
  absent; CI retains that compiler lane. M4 remains incomplete for special-action
  ledge predicates, broader action-specific ECB evolution, other partial
  fidelity rows, and the native hands-on controller gate.

## 2026-08-10 Falcon Dive bidirectional ledge callback

- The required prior-art sweep compared pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` with current head
  `6ddd74ecbb755df25b32f137b5f7b7f6d7005e91`. The relevant Falcon Dive and
  common ledge collision files are unchanged. `doAirColl` passes direction
  zero to `ft_CheckGroundAndLedge` after its command-variable gate, unlike
  ordinary Fall/FallSpecial paths which pass facing.
- Production now computes one action-owned ledge-probe policy and reuses the
  existing allocation-free catch path. Falcon Dive start accepts both sides
  only after the imported gate, throw makes no source-absent ledge query, and
  the selected endpoint assigns inward facing on catch. No second collision
  routine or character switch was introduced.
- A deterministic physical route reaches Final-Destination-like left geometry
  through normal walk/jump/descent input, starts aerial Falcon Dive, reverses
  facing at the source command gate, then catches the ledge behind Falcon. It
  asserts outward facing before contact and inward facing plus zero velocity on
  `LEDGE_CATCH`. Windows and WSL movement suites pass.
- The retained source capture still verifies exact facing-toward behavior:
  63 Dive frames, frame-64 `EdgeCatch`, frame-71 `EdgeHang`, source SHA-256
  `5a5b295d0fc7a8d1c06512dc704176a131a7c01a931a0a2b92f6d7ff8c3a8295`,
  and all 63 production samples within the established Q16 envelope.
- The broader Falcon Dive pack exposed a pre-existing red aerial-catch lane.
  Automated Git bisect identifies `821fde3` (`Match Falcon sphere shield
  collision`) as first bad: its correct feet-origin-to-reference-joint geometry
  conversion invalidated an accidental same-frame collision between a source
  route pinned at `y=500` and a native natural-jump setup. The product transform
  remains intact; the lane must be split into exact-position geometry replay
  and genuinely identical-input dynamics.
- The owner ISO is reachable again. A headless/null/unlimited source capture
  now closes the facing-away branch: Falcon turns outward on Dive frame 13,
  remains outward through frame 63, catches on frame 64, turns inward, and
  enters `EdgeHang` on frame 71. All 63 production samples pass within the
  established Q16 envelope; raw SHA-256 is
  `026faf91c3582aa5e41c5d95ba757904ec7ef7865a049994ce169f70a6157009`.
  The shared source verifier accepts both route identities and validates their
  distinct facing histories plus the same collision-memory ledge predicate.

## 2026-08-10 Falcon Dive exact-position aerial-catch theorem

- The previously red aerial-catch comparison mixed a source route with pinned
  fighter positions and a native route with natural jump motion. It is now
  split at the correct boundary: identical-input dynamics remain in the live
  verifier, while collision geometry replays the exact observed relative
  placement through the reusable stored-oracle runner.
- A collision-authoritative opponent extractor imports Falcon JumpF displayed
  frames 9-20 from the post-transition rows of capture SHA-256
  `59a4489ea6e955c9bb587bb5e49bc5d34ce4cce6ae42accd98a24ff97e271a6f`.
  Production consumes the same immutable 12-pose track through the existing
  common hurt-pose accessor; the source and production pose serialization both
  hash to `0409481263f9d10432b7e6f7ccd7e10d7b8cce69e3ba05676170cf3fc4fa9254`.
- The generic geometry-case schema owns attacker move/frame, exact target
  offset, facing, and grabbability. Falcon Dive's observed placement hits with
  margin `+4.645228676`; the translated control misses with margin
  `-1.151280430`. Both use the same allocation-free C predicate as the prior
  common-hurt collision theorem.
- The fast registry now covers 12 domains / 81 cases plus replay in 1.241
  seconds on Windows and 1.234 seconds on WSL. Windows GCC Release, WSL
  Release, focused WSL ASan/UBSan, Python syntax checks, regeneration checks,
  and the repaired combined Falcon Dive verifier pass.

## 2026-08-10 generic Falcon Punch stored oracle

- A fresh prior-art check compared pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` with current upstream
  `6ddd74ecbb755df25b32f137b5f7b7f6d7005e91`. Falcon Punch remains governed
  by its ground/air animation, physics, and collision callbacks; no maintained
  implementation displaced the existing live capture route.
- The two existing direct Dolphin captures remain the source of truth:
  ground SHA-256
  `2c8bc604024cfad745e266239dcc4d3e1b1ff1c4a07afcc6eecb9938b5f155b1`
  and air SHA-256
  `9cfc8c5632a8bce37a0f79c6999bff6f0742130df5f8f2473196338d8b14d6c5`.
  The repaired at-will verifier rebuilds its strict native runner, passes both
  200-frame comparisons, and now verifies the source projection as one gate.
- The reusable numeric stored runner adds three cases / 251 samples: complete
  ground physics and clock, complete air clock, and air physics from frame 50
  through the first Fall sample. Splitting the air route preserves the live
  comparator's frame-49 reanchor and avoids claiming unqualified prefix
  physics. Source/production SHA-256 are
  `defbb9746b3784c6e1aae2b7d176344fadcb9a1ba0c51ac4b8c097e1765a16f1` /
  `eec11ed8d9050fe51196b9241e326c3e189be8a56dad82e64b5ee28a7c5b527e`.
- The generic registry now covers 13 domains / 84 cases plus replay. The full
  post-build gate passes in 1.065 seconds on native Windows and 1.366 seconds
  in WSL, both below its 2-second budget; the focused CTest takes 0.05 seconds
  on each platform. The importer skill now records the reusable partial-prefix
  projection rule. M4 remains incomplete for the other partial fidelity rows
  and the native hands-on controller gate.

## 2026-08-10 generic Raptor Boost stored oracle

- A fresh prior-art/decomp sweep found no current callback change or maintained
  exact stored-regression runner that supersedes the existing live path. The
  full 657-frame at-will verifier remains green: five fighter routes plus the
  source-native grounded Capsule search.
- The new generic `native-csv-trace-v1` schema reuses the production
  `pf_m4_movement_trace` binary. It generates compressed input runs and exact
  field/exclusion metadata instead of another Falcon-specific C adapter.
- Shared controller normalization and shared Raptor action/timer mapping prove
  the stored inputs and source projection are the same ones used by the live
  comparator. Per-field row exclusions retain all qualified action-clock rows
  and omit only the live-unqualified hitlag and special-landing samples.
- Five cases / 502 samples are pinned under source/production SHA-256
  `19b5d604d5721e20bc2151e41c11054632a5c384dfd5528cf373dac2bd1abe2c` /
  `7733655e234ac2de12fe1b674ed6be967ad7de39b848d94cf97b2e36547509a0`.
  The Capsule route remains live-only because original Relay Rod content is not
  a source Capsule.
- Independent native cases now run concurrently while canonical results retain
  manifest order. The 14-domain / 89-case gate plus replay passes six Windows
  runs in 1.435-1.550 seconds and three WSL runs in 1.448-1.585 seconds, below the
  two-second budget.
- The focused native-CSV domain is part of ordinary CTest and therefore the
  GitHub Actions native matrix. Windows and WSL Release pass 33/33 tests; the
  added test takes 0.36 seconds on each host.

## 2026-08-10 natural airborne landing equivalence

- The first divergent natural frame was JumpAerialB entry: source horizontal
  velocity was `-0.396`, while production retained only the imported `-0.36`
  jump impulse. Pinned decomp shows the interrupt transition completes before
  the fighter's physics callback, so the new JumpAerial state applies ordinary
  air control in that same update. The shared double-jump entry now preserves
  that order; no Falcon-only movement branch was added.
- A second divergence was a one-update-late FallAerial landing. The owner
  capture already exposed complete eight-frame Fall and FallAerial loops, so
  the importer now packs those with all four jump tracks: six tracks / 186
  poses under profile/semantic SHA-256
  `407a62269b2aa65002bb4a78152f12a49b56d36d8b68a684c6d55a11ce69a1ba` /
  `21a2d02fbb3abfcd9c29bb170c4c378fc8972fe191098fb5587140e965dac25a`.
- Two independent 520-frame Battlefield captures have different raw JSON
  hashes because process-local addresses and nonsemantic probe values differ,
  but their canonical controller/action/physics traces are identical. Windows
  and WSL both pass all 520 identical-input frames with strict discrete and
  velocity fields and the documented 640-Q16 position envelope. Source and
  production SHA-256 are
  `43eb893b7d70852b03b696c993db09e08ee4554bee5922c2a214aef73da7bf95` /
  `9e8fd5b0c3ee8d065c0fef6c906c9700d1600d6ef34e7eb498735a25ed81f26b`.
- The route is the fifteenth generic stored domain and raises the registry to
  90 cases. Three full Windows runs take 1.592-1.678 seconds; three WSL runs
  take 1.279-1.578 seconds, including deterministic replay and all generated
  checks. The new replay identities are corpus
  `540695baf9fa0bce01ac9342310f78ecd40b5406bd03df57a81e8b557663d798`
  and final state
  `4ffccdd98a49489adf6737f54d5d987bc1c591c71cb1d39aa53d33f2e9c630f6`;
  the event digest remains unchanged.
- The first CI headless run exposed a test-choreography collision rather than a
  match-system defect: player 1's earlier attack recovered to idle on the same
  tick that player 0 lost its final stock, correctly producing transition mask
  `0x03` where the assertion expected only player 0. The fixture now waits for
  the setup attacker to return to idle before the final-stock phase. All 27
  WSL headless tests and the strict Windows MSVC match suite pass.

## 2026-08-10 Falcon Kick stored qualification and domain-level acceleration

- The current upstream decomp has no Falcon Kick state-table, attribute, or
  callback change relative to pinned revision `9509dc0`. The existing six
  hash-pinned live captures remain the source owner.
- Falcon Kick now reuses the generic `native-csv-trace-v1` stored path. Shared
  pure helpers own action/timer projection for both the live comparator and
  source verifier; the manifest owns exact inputs, fields, and sparse hitlag /
  Hyrule wall masks. Six cases bind all 399 live-qualified rows under source /
  production SHA-256
  `2c6f28a9701990b913adb2f2daa214433bb18174a610af6c96fc1dce39deaf33` /
  `19a4dd302f0e51fa9d01d8fe7193d57e1b3ea5979e496fb6138bc0c85f356f4e`.
- The mandatory live rerun caught a first-frame ground-edge discrepancy before
  the new production digest was accepted. Ground-origin Falcon Kick now keeps
  its source zero-velocity/no-gravity aerial-end conversion update; all six
  live scenarios pass again for 70/59/65/70/77/58 frames.
- The corrected conversion shifts the intentional seeded verifier soak by one
  tick. Windows and WSL independently reproduce eight matches, 2,848 total
  ticks, and digest `b9239b63a68a1a18`; that content-bearing golden is repinned
  while the separate deterministic replay corpus remains unchanged.
- The root stored gate now runs independent domain generation and execution
  concurrently and restores manifest order before counting and hashing. The
  full 16-domain / 96-case gate plus replay fell from 1.758-1.819 seconds to
  0.430-0.508 seconds on Windows and 0.384-0.408 seconds in WSL. Focused
  movement, combat, and Falcon Kick CTests pass on both hosts.

## 2026-08-10 aerial-attack ECB and natural landing qualification

- A current-upstream decomp sweep rejected the initial aerial-ledge
  hypothesis: `ftCo_AttackAir_Coll` performs floor collision but does not call
  the ordinary Fall callback's ledge-acquisition branch. Production therefore
  continues to exclude ordinary aerial attacks from ledge catch.
- Two independent no-fast-forward captures reproduce all 195
  Nair/Fair/Bair/Uair/Dair ECB poses under semantic SHA-256
  `55e686a07cf3d064618104051f0085ed2a398e9a1612847200b2cba51a665f10`.
  The importer generates one packed bottom table for the floor-only callback;
  the checked-in full profile retains all four ECB points as source evidence.
- Natural short-hop routes found the first production divergence at Nair's
  platform contact. Animated ECB sweeps now preserve the previous action pose
  and the current post-callback pose, and imported aerial ECBs accept either a
  direct crossing or only the immediately preceding bounded crossing when the
  source contact flag is one update late. This cannot snap an arbitrary
  already-below fighter upward.
- Nair/Fair/Dair landing lag and Bair/Uair auto-cancel now pass all 685 source
  frames twice. Shared source projection replaces duplicate one-case verifier
  logic. Five generic concurrent stored cases are pinned under source /
  production SHA-256
  `83e1fadc017af2c5005411ea2fae8d378855127662196fa0b9b81a37c7a11efe` /
  `dd14d194d15a115925c17761fd5fff692413b26a268ef96286180e176273e490`.
- The complete registry is 17 domains / 101 cases plus replay: 0.575 seconds
  in WSL and 1.458 seconds on native Windows, below the two-second budget.
  WSL and Windows movement/combat/replay, deterministic generation, and WSL
  ASan/UBSan pass. Three stable replay reruns pin corpus/final/event SHA-256
  `0d3ccb293d0735102c13d020d469f13b202eede2b54052881d0380efb765e172` /
  `3a9bb1e28fd635dcde8f1ec98d0705babd12ee64ee7e036e8f986c5a15a874d5` /
  `370975f72bbd6546f5253607ef62b811cb4f126889ad3c89bf4b2955703430cb`.

## 2026-08-10 complete ordinary-airborne hurt poses

- A current-upstream `doldecomp/melee` sweep reconfirmed the six source
  submotions retained by ordinary airborne movement. The existing 494-row
  airborne-ECB choreography already covers every displayed frame, so no new
  scenario or technique-specific test was added.
- Two independent headless/null/unlimited captures disabled fast-forward and
  enabled the collision-authoritative hurtbox probe. Their distinct raw
  SHA-256 values canonicalize to the same 186-pose / 2,046-capsule semantic
  SHA-256
  `71c9e643816604f9d2e90cfc226b907e7ce7cb48edc4fa2fea51d6797013ee7f`.
- The generic extractor gained per-track label projection so looping Fall
  actions cannot absorb setup rows from the next checkpoint. One generated
  immutable table now serves JumpF, JumpB, JumpAerialF, JumpAerialB, Fall, and
  FallAerial through retained source-submotion lookup.
- Falcon Dive's existing hit/miss theorem now consumes JumpF frame 20 from
  that complete table. The 12-frame Dive-only JumpF profile, binding, and
  generated include were removed, eliminating 132 duplicated capsules.
- The seventeen-domain / 101-case registry now hashes 875 production poses
  and completes in 0.433 seconds on WSL and 0.581 seconds on warm Windows. Both
  platforms pass the full stored gate, movement, combat, and deterministic
  replay without changing the pinned replay hashes.

## 2026-08-10 velocity-driven Falcon ground-loop poses

- Canonical Falcon state now retains the source submotion, fractional animation
  cursor, and rate required by Melee's velocity-driven WalkSlow/Middle/Fast and
  Run callbacks. Walk gait changes use the decomp phase-remap equation; the
  shared stored numeric runner protects three clock cases / 111 source samples.
- A prior-art sweep covered the pinned `doldecomp/melee` HSD runtime, the
  repository's existing FigaTree parser, and current HSDLib before extending
  the smallest reusable source-data path. The corrected generic importer maps
  the complete 63-joint active gray-costume Falcon model and 63-entry runtime
  part layout to a 25-joint parent-closed subset shared by all 11 hurt capsules
  and six ECB selectors, then emits 229 FObj tracks / 1,295 keys across the
  four ground-loop motions.
- Exact `HSD_FObjReqAnim` behavior matters: each track's `startframe` is added
  to the requested animation frame. Preserving the older subtractive helper
  interpretation displaced WalkMiddle because two translation tracks start at
  frame 52. The shared Python and C evaluators now use the additive rule.
- A fresh 423-row headless/null/unlimited checkpoint pack records live motion,
  animation, and six-frame default blend state. After blend completion, the
  source evaluator agrees for all 51 WalkSlow, 31 WalkMiddle, 29 WalkFast, and
  20 Run observations: 131 poses / 1,441 capsules with maximum 2-Q16 coordinate
  error. Eight stored observations / 88 capsules independently protect the
  production C evaluator.
- Portable MSVC Release passes 35/35 with bounded two-way scheduling; WSL
  Release passes 35/35, and WSL ASan/UBSan passes 25/25. The twenty-domain /
  116-case stored gate plus replay completes in 0.677 seconds on Windows and
  1.308 seconds in WSL. A five-repetition native benchmark records 983,690
  representative 1v1 ticks/s and 568,368 maximum-combat ticks/s. The rebuilt
  Emscripten playtest reaches a live tick-50 match with clean browser logs.

## 2026-08-10 WalkFast live qualification

- The grounded-loop route now enters Walk below the dash threshold, then raises
  libmelee's normalized stick from `0.85` (source `+0.7`) to `0.95` (source
  `+0.9`). This crosses Falcon's decomp-derived `0.8 * walk_max_vel` gait
  boundary while the Walk callback owns selection, naturally reaching
  `WalkFast` without an action or velocity override.
- The resulting 423-row headless/null/unlimited capture adds 29 post-blend
  WalkFast samples. The shared DAT evaluator now passes 131 live fractional
  poses / 1,441 capsules across all four ground-loop motions with a maximum
  2-Q16 coordinate error. Two WalkFast discriminators extend the production C
  oracle to eight observations / 88 capsules.
- An independent repeat capture reproduces the same 131-sample source result
  and the unchanged 278-pose CrouchWait/Appeal semantic SHA-256. The two raw
  capture digests and warm timings are pinned in the coverage manifest.
- Generic checkpoint projection now also supports label-delimited packs that
  intentionally have neither `capture_shards` nor a character-specific
  `*_cases` list. `--oracle-case walk-fast-clock-probe` retains the complete
  restore-delimited setup/input segment but emits only its 32 WalkFast rows;
  the measured warm capture falls from 4.820601 seconds for all seven cases to
  0.160820 seconds for the focused case. The output embeds a one-case projected
  manifest with the full-pack row expectation removed.

## 2026-08-10 Falcon shield-break orientation

- The common decomp does not always select ShieldBreakDownU: its landing route
  evaluates terminal ShieldBreakFly `HipN->mtx[1][1]` and chooses DownU only
  when positive. Falcon uses that ordinary predicate, and the pinned DAT pose
  evaluates to `-3921` Q16, selecting DownD.
- The generic dynamic-HSD importer now supports compact terminal-pose matrix
  predicates. It emits only the source component and boolean needed at runtime;
  Falcon's landing path selects DownD and carries the same orientation into
  StandD without a duplicated motion evaluator or additional rollback state.
- Two independent 500-row headless/null/unlimited captures drain a real digital
  shield and reproduce 42 ShieldBreakFly, 26 ShieldBreakDownD, 30
  ShieldBreakStandD, and 127 ShieldBreakTeeter observations in exact order. The
  source verifier recomputes the DAT predicate and validates both raw capture
  digests, disc identity, oracle artifact, action IDs, sequence, and counts.

## 2026-08-10 Falcon shield-break physics and canonical state

- Two fresh surface-probed controls expose all 42 ShieldBreakFly ECB frames.
  Production now evaluates those immutable per-frame left/right/top/bottom
  offsets during floor collision; this moves contact from the former frame 40
  to the source frame 42 without changing Falcon's already-correct launch
  gravity or terminal velocity.
- The decomp air-to-ground helper preserves `self_vel.y` on the first
  ShieldBreakDown row, and grounded physics clears it on the next row.
  Production retains that one-update transition value instead of hiding the
  mismatch in the velocity tolerance.
- Shield health now preserves the source entry distinction: passive depletion
  enters Fly at zero, while `Fighter_ProcessHit` assigns the common reset value
  before a hit-induced break. Global regeneration runs through Fly/Down/Stand;
  Furafura resets to 30 before the same 0.07-per-tick global update.
- Both independent 500-row captures pass identical-input native comparison for
  action, clocks, facing, grounded state, invulnerability, vertical position,
  velocity, shield health, and strength. The bounded Q16 envelopes remain 640
  for accumulated position, 32 for velocity, and 64 for shield health.
- `falcon-common-shield-break` is the twenty-first generic stored domain. Its
  500-sample production trace is pinned as one compressed-input case, raising
  the registry to 117 cases without adding a Falcon-specific runner.
- The content-bearing seeded verifier soak changes deterministically because
  shield health is part of canonical state. Windows and WSL independently
  reproduce eight matches, 3,002 ticks, and digest `fc0c77b3bfcf5c24`; the
  separate replay corpus remains unchanged.
- The Emscripten client rebuild and real headless-Chrome smoke pass. The smoke
  script's replay assertion is synchronized with the already-enforced current
  canonical final SHA-256 `f04b6ff2ff80bf5dba91788ce69e0b62f0e394047a60928812943a0613c55637`.

## 2026-08-11 fractional ground-loop ECB source ownership

- The required prior-art sweep compared pinned and current Melee decomp
  animation/collision code, current HSDLib, the repository's HSD evaluator,
  and existing live captures. The retained-channel interpretation recorded in
  the first version of this section was disproved by the costume probe below.
- The surface probe reads the six source-authoritative ECB JObjs plus their
  parent-closed 25-joint local SRT/world-matrix closure directly from Melee's
  `CollData.ecb_source` and `Fighter.parts`. Four parallel checkpoint workers
  capture all 143 WalkSlow/Middle/Fast/Run rows in 7.48 seconds warm; a focused
  WalkMiddle run takes 4.66 seconds including Dolphin launch/menu setup.
- Player 1 reports costume ID 1 and the live costume-root pointer. Falcon's
  decomp costume table maps ID 1 to gray `PlCaGy.dat`, while the original
  evaluator incorrectly used neutral `PlCaNr.dat`. Live leaf translations 25
  and 47 exactly match the gray model. `ftAnim_8006FA58` and
  `ftAnim_8006FB88` copy the active costume bind SRT into the current or target
  skeleton before attaching FigaTree tracks, so omitted post-blend channels
  resolve to costume bind values rather than arbitrary preceding-action state.
- The generic importer now parent-closes the six ECB selectors into the same
  25-joint catalog used by the 11 hurt capsules. Production evaluates both
  consumers through one Q16 HSD matrix core and reproduces grounded
  `mpColl_LoadECB_JObj`, including the strict less-than-10 symmetry branch and
  its side clamps. WalkSlow/Middle/Fast and Run production routing is enabled.
- Two independent captures each qualify 131 post-blend poses. Across 262 live
  poses / 2,882 capsules the maximum hurt-coordinate difference is 2 Q16 and
  the maximum ECB difference is 1 Q16. Eight fractional and loop-adjacent
  observations store both outputs and protect the C evaluator.
- A separate four-worker transition pack records 193 rows per capture and the
  current plus target local SRT for the same parent-closed 25-joint catalog.
  Two independent captures verify 54 adjacent moving-target blend updates /
  1,296 joint recurrences against `ftAnim_8006E9B4`, `ftAnim_8006FE9C`, and
  `lb_8000C490`, including a nested gait change. Maximum differences are
  `1.2330335e-6` for translation/scale and `2.20395109e-7` for quaternions;
  158 converged rows are pinned by semantic SHA-256
  `cd3aba1802a0b749e2b677e720eadb4c97b254574b548f184be529589ef16f1d`.
  Production now uses one compact rollback representation: 19 canonicalized
  Q15 quaternion xyz triples, six Q16 translation triples, and one Q16 blend
  progress value per player. A generated 27-row C oracle replays every
  captured moving-target recurrence from the repeat capture, including nested
  gait changes and accumulated compact re-quantization. It passes at maxima of
  8 Q15 quaternion units and 4 Q16 translation units. Hurt geometry,
  inspection, and wall ECB all reconstruct through the same local-pose core.
- Five checkpoint-equivalent production cases call the real `pf_sim_tick` path
  at Wait-to-Walk, two gait changes, a nested gait change, and Dash-to-Run.
  They exposed the missing pre-IASA animation update: Melee advances the old
  animation and any active old blend before selecting the replacement action.
  One shared continuation helper now owns both ordinary blend continuation and
  transition-source reconstruction. All five production outputs match the live
  compact poses within 4 Q15 rotation and 4 Q16 translation units.
- Canonical state schema 71 / save format 66 (`PFSAVE60`) serializes the
  compact transition state explicitly. The payload is 1,567 bytes and the
  complete checkpoint is 1,707 bytes; inactive blends serialize as canonical
  zero. Replay grows by exactly the added 760 state bytes to 42,479 bytes.
- Windows Release passes 40/40, WSL Release passes 40/40, and WSL ASan/UBSan
  passes 26/26. The 21-domain / 117-case stored-plus-replay gate passes in
  1.018 seconds on Windows and 0.686 seconds in WSL.
- A clean WSL clone measures exact parent `685afea` and corrected commit
  `32aca0c` on one warm toolchain/build fingerprint. All ten available
  scenarios compare as compatible with zero suspected or confirmed
  regressions. The corrected commit records 1,014,990 representative-1v1 and
  647,699 maximum-combat ticks/s. The host provides GCC 13.4.0 rather than the
  pinned 13.3.x compiler, so both sides use the explicit unpinned-compiler
  override and record the actual compiler identity.

## 2026-08-11 DAT-driven Raptor Boost ECB ownership

- The prior-art sweep covered pinned/current Melee decomp collision and
  animation code, the existing HSD evaluator, all retained Raptor captures,
  and owner-extracted Falcon DATs. The existing evaluator was extended rather
  than introducing a second motion parser or another route-specific table.
- The generated HSD profile now carries ten motions / 580 tracks / 5,346 keys:
  six compact Wait/Walk/Dash/Run blend motions and four direct-only Raptor
  Boost start/hit motions. Direct-only motions cannot enter compact rollback
  channel generation, so the persistent 19-rotation / six-translation blend
  representation does not grow.
- The shared ECB path evaluates the six source JObjs plus TransN, subtracts
  the reference origin, and implements `mpColl_LoadECB_JObj` grounded and
  airborne policies. It also reproduces the first-four-update airborne bottom
  lock and keeps desired bottom separate from locked actual bottom when
  deriving side Y.
- Hit transitions expose Melee's callback ordering: displayed action frame
  zero still owns the source start pose that entered the state, and hitlag can
  repeat that old pose. Production binds the Raptor source submotion and
  animation clock before collision, retains the entry source during hitlag,
  and only then lets the new hit animation advance.
- Five manifest-bound captures contribute 423 observations across 411 complete
  action frames. The independent source evaluator agrees with live Dolphin
  within one Q16 unit. Four selected C poses agree within 32 Q16 units under
  deterministic fixed-point matrix evaluation, and the complete 657-frame
  production Raptor verifier passes.
- The old 45-value `SpecialAirS` bottom-only table and its importer digest
  member are removed. The complete Falcon source digest is now
  `af61fad2387f92067a0fc6eaefbd8322fa9fa0401e8281c4618e6949cf1c44f1`.
  This exposed a pre-existing one-frame floor-contact bug: an exact-parent
  A/B run fails on the same aerial-hit landing row, while the source-derived
  complete ECB fixes the route.
- The focused Raptor script had independently drifted from the production
  translation units and failed to link at the exact parent. Its manual compile
  now includes the shared fixed-math and HSD-pose sources; this is harness
  repair, not a simulation-behavior change.
- Final local gates pass: Windows Release 38/38, WSL Release 40/40, WSL
  ASan/UBSan 26/26, and the full 21-domain / 117-case stored-plus-replay lane
  in 0.705 seconds on Windows and 0.586 seconds in WSL. Deterministic Falcon,
  dynamic-HSD, stored-Raptor generation, and Python bytecode checks also pass.

## 2026-08-11 Falcon Dive and FallSpecial DAT/HSD ECB ownership

- The shared HSD profile grows from ten to 17 motions / 1,204 tracks / 10,106
  keys, adding Falcon Dive ground/air start, catch, throw, and common
  FallSpecial neutral/forward/back without adding another pose evaluator.
- Nine retained source captures qualify eight motions, 733 rows, and 715 unique
  source frames. The independent DAT/HSD evaluator remains within two Q16
  units of Dolphin across all complete four-point ECBs.
- Production reproduces common Fall direction selection from imported speed
  and common-data thresholds. A target switch executes the decomp's install
  blend and then its ordinary advanced-target blend; stable target updates
  execute only the latter. The observable switch bit and bottom-lock state are
  canonical rollback state rather than inferred from later geometry.
- A fresh headless/null/unlimited `up_air_miss_natural` capture has SHA-256
  `55ddf2eed8a8cd52d788075b62bca1e6d7be3a1a26ce89bec6e02b85475ea5b4`.
  Production passes all 165 identical-input frames under the reviewed
  640-Q16 position envelope with strict actions and velocity channels.
- The authored Raptor Boost, Falcon Dive, and FallSpecial ECB arrays are gone.
  FallSpecial loop length comes directly from the imported submotion catalog.
  The regenerated include SHA-256 is
  `754a72159e5463752e382dd6a2a8e35657bab601b84c228ade5c540d30272a74`,
  and complete-source digest is
  `74e4a7b7a8635a870ba65cb77425eb3500d0df2d53b41fa1ff9aa9fffbb3edee`.
- State schema 74 / inspection schema 56 now serialize and expose the required
  transition fact. Save size is 1,747 bytes and deterministic replay size is
  42,519 bytes. The full WSL Release suite passes 40/40 in 5.67 seconds and the
  strict native Windows MSVC suite passes 40/40 in 8.35 seconds.
## 2026-08-11 ordinary guard pose fidelity

- The pinned decomp shows that Falcon `GuardOn` is not its raw FigaTree.
  `ftCo_80091E78` manually blends Wait toward the shield skeleton across eight
  `Ft_MF_SkipAnim` updates, `Guard` freezes that terminal pose, and `GuardOff`
  advances normal HSD frames 0-15. The generic hurt and ECB extractors now
  support explicit observation-order and fixed-frame tracks for this reusable
  hidden-clock pattern.
- Two independent 40-row headless captures regenerate identical 25-pose hurt
  and ECB semantics. Hurt profile / semantic SHA-256 values are
  `e1d76de8fac684d0976fa464d5906a145aeb340a99c0f88490da3574231c763b` /
  `4db8c524835e969b5b34fda81e53b59d6af99aa68d13e7203086c6441a41abde`;
  ECB profile / semantic values are
  `4ac108b18b77438b84760dd0dbea1ac830e8b5f323429aaeb01ecd4b66e48165` /
  `a1bd5b9937cb342a053415ecc674b36dc5a01fb575ed688b32f8e097e1b209c1`.
- Production retains GuardOn, Guard, GuardOff, and GuardSetOff source identities
  through movement, combat, snapshot validation, save/load, and hitlag. The
  exact O(1) accessor supplies 275 hurt capsules and 25 complete four-point
  ECBs without new canonical state. GuardSetOff's dynamic-rate geometry is
  deliberately not claimed by this slice.
- The common-hurt physical manifest now declares all 16 boundaries actually
  executed by the checkpoint pack. The checkpoint runner reapplies position
  and RNG writes after restore acknowledgement; previously those writes could
  target the discarded pre-restore state. Two corrected full captures produce
  the same 714-pose source SHA-256
  `9688be9b0ca0d0eacac5ba26714968acdcc3b19aaac9449778c108275b1c940b`,
  and the live dash discriminator passes at margins `+0.289212401` /
  `-0.156798480`. The production accessor SHA-256 is
  `0641ed13ea1d179e214f5629b4f8d7b93e226091b9925d6e31dfe53645c74c36`.
  Correct full-pack warm measurements are 14.10-15.05 seconds for 323 rows;
  focused guard-only qualification remains about 0.15 seconds warm.
- The canonical source-submotion field now records guard identity, so replay
  bytes change while the tick-240 final-state and event digests remain stable.
  The reviewed replay corpus SHA-256 is
  `e327a55b18221e35eb106a70de7cae48db1c38068e38fa13fc9680ba4e4759a4`;
  two independent verifier soaks reproduce digest `83f2bdc274c8920a`.
- Strict Release gates pass 38/38 on Windows in 3.54 seconds and 40/40 in WSL
  in 3.12 seconds. The full 21-domain / 117-case stored-equivalence plus replay
  gate passes in 910.252 ms on Windows and 814.570 ms in WSL, below its
  two-second post-build budget.

## 2026-08-11 Base Wait source pose fidelity

- A focused native SquatRv entry captures exactly Wait frames 0-59 twice.
  The direct, post-blend frames 6-59 produce 108 observations / 1,188 hurt
  capsules plus ECB and match the shared compact HSD evaluator within one Q16
  unit. Three stored C observations cover frames 6, 30, and 59.
- Production now retains the Wait submotion and advances its canonical
  animation clock at rate 1. Dynamic source hurt/ECB geometry begins at frame
  6; the incoming six-frame blend remains open. Terminal Wait2/Wait3 selection
  remains open because it consumes process-global `HSD_Randi` state.
- The new idle cursor changes intermediate replay hashes without changing the
  tick-240 final-state or event stream. The reviewed replay corpus SHA-256 is
  `0614479949db01ac2e8a0c13e6998103760f62e0e8c7681ee4f6553d765915e4`;
  final/event SHA-256 values remain
  `77ddce5af5e91b4c4e5d91bebcc3c4ad73ca0dc214262fbbc922222799033f33` /
  `6f0f9376198d1f9507e6502da4eece00110a6ebe7c18233c78303d5b9764743d`.
- Windows Release passes 40/40, WSL Release passes 40/40, and WSL ASan/UBSan
  passes 26/26. The complete 21-domain / 117-case stored gate plus replay takes
  832.743 ms on Windows and 1,001.002 ms in WSL.

## 2026-08-11 GuardSetOff dynamic pose fidelity

- `ftCo_80092F2C` derives GuardSetOff's ordinary animation rate from
  `(0.1 + endpoint) / unrounded_shield_stun_duration`; the imported Falcon
  endpoint is 20. Production now keeps that fractional clock separate from
  the integer shield-stun countdown.
- The canonical source-animation cursor is reused with source callback order:
  hitlag freezes the receiving GuardOn/Guard collision pose while retaining
  the dynamic rate, and the first resumed update switches to GuardSetOff and
  advances once. No rollback field, runtime allocation, parser, or host float
  was added.
- Light, midpoint, and dense shield-hit routes were captured twice through
  headless/null/no-fast-forward ExiAI. Their address-free semantic repeats are
  identical; the shared HSD evaluator passes 18 live updates / 198 capsules
  with exact hurt coordinates and maximum one-Q16 ECB difference.
- The parent-closed source profile expands to 22 motions, 1,574 tracks, and
  12,784 keys under SHA-256
  `386f7caf986b582363efc79aaf2efda04a93b812f9f3565ef62c6690eefe6e1b`.
  The reusable multi-capture generator emits nine stored pose/ECB observations,
  while the existing shield production test owns rate and phase integration.
- The identical-input comparator replays all six captures from the explicit
  22-unit placement boundary. Six 99-frame comparisons pass and directly
  check 36 live/native GuardSetOff clock rows. The native runner now retains
  imported Jab 1 timing/source spheres instead of its obsolete two-active-frame
  authored override.
- Windows Release and WSL Release pass 40/40. WSL ASan/UBSan passes 26/26.
  The full 21-domain / 117-case stored-plus-replay gate passes in 886.587 ms on
  Windows and 823.440 ms in WSL. Two verifier runs reproduce deterministic
  digest `e52299dd5cd51ec8`.

## 2026-08-11 SquatRv-to-Wait transition fidelity

- Two fresh 70-row headless/null/unlimited hurt captures cover ten displayed
  SquatRv updates followed by all 60 base-Wait updates. The shared DAT/HSD
  evaluator matches both captures across all ten SquatRv poses and direct Wait
  frames 6-59: 128 observations / 1,408 hurt capsules plus ECB, with maximum
  one-Q16 coordinate error.
- A separate pair of surface-memory captures proves Wait frames 0-5 as
  Melee's exact six-update moving-target recurrence. The hidden source is
  SquatRv frame 10, one update after its last displayed frame; the target begins
  at Wait frame 0 and advances one frame per update. The paired 140-row theorem
  passes 210 joint updates at maximum local error `4.76837158e-07` and
  quaternion error `1.71211921e-07`, with semantic SHA-256
  `0c7ba43ab7022bc2e88bcb369e4fcb9812ebf2397abd7afddfed931e61734983`.
- Production retains SquatRv's real source clock through CrouchEnd and uses a
  compact replay descriptor to reconstruct the wide-SRT blend on demand. This
  avoids storing six full translation vectors per player: the in-memory compact
  pose grows only four bytes, while save format 66 remains 1,747 bytes and the
  replay remains 42,519 bytes. State schema advances to 75.
- Six generated Dolphin observations check every natural blend update through
  the real tick, hurt-capsule, inspection, and ECB paths. The compact resolver
  accepts only the bounded one-frame/six-update descriptor, so malformed saves
  cannot request unbounded replay work.
- Windows Release passes 40/40 in 11.19 seconds, WSL Release passes 40/40 in
  9.81 seconds, and WSL ASan/UBSan passes 26/26. The complete 21-domain /
  117-case stored gate plus replay takes 970.038 ms on Windows and 925.398 ms
  in WSL. Replay corpus/final/event SHA-256 values are
  `f7f59a2f68b3431ff459fb8342684f4701e936cb6f83298834adddcbc365a49e` /
  `f7697243c6a07965e31224c54f015798bef6d615ce6e2d1441576d6f1450f98b` /
  `6f0f9376198d1f9507e6502da4eece00110a6ebe7c18233c78303d5b9764743d`.
- Base Wait's incoming geometry gap is closed. Process-global `HSD_Randi`
  state and exact Wait2/Wait3 selection ordering remain the next idle-lifecycle
  gap and are not claimed by this slice.

## 2026-08-11 complete Wait idle lifecycle

- A prior-art/source sweep of pinned and current `doldecomp/melee` confirms the
  character-owned weighted wait table plus shared `HSD_Randi` route. Falcon's
  extracted DAT supplies Wait/Wait2/Wait3 weights 70/20/10 and blend bytes
  6/0/0; the shared HSD RNG uses the exact 32-bit `214013`/`2531011` LCG and
  high-16 bounded selection. A secondary idle that selects itself consumes a
  retry draw, while base Wait may restart.
- The checkpoint harness isolates the opponent in crouch, writes the initial
  source seed on displayed base-Wait frame 59 immediately before selection,
  and reasserts robust manifest-owned endpoint seeds so unrelated global HSD
  consumers cannot change the intended observed route.
  Two 440-row headless/null/unlimited captures are byte-identical at SHA-256
  `d97474f2a15912b1c98fba9b7444883c1db4798290702c311b13bcffb4cc7f7b`;
  their address-free semantic SHA-256 is
  `afefafe17e8769bc39391d0605d7c392f25ef4d146cf0d868eb895eeee84b570`.
  The source verifier checks 880 live rows plus a separate decomp/production
  theorem covering 14 uninterrupted draws and two rejection draws, all weights,
  and exact 60/75/70-frame clocks.
- Wait2 and Wait3 extend the existing immutable HSD profile to 25 motions,
  1,770 tracks, and 16,092 keys under decoded-data SHA-256
  `08bb5d58ac69e9112cc5d1a63a9ec91ad06c7c2cb6deb8ebcc1e3b78804d3781`.
  All 145 direct source poses / 1,595 capsules plus ECB match Dolphin within
  one Q16 unit. Thirty-two stored poses cover entry/middle/terminal and every
  return-to-base/base-restart blend update.
- Production retains the global RNG in canonical rollback state, stages draws
  transaction-locally, and commits only a successful tick. RNG behavior
  version advances to 2 without adding per-player state. The ordinary
  moving-target blend resolves the evaluated terminal-plus-one source pose;
  shared replay descriptors cover both idle-to-idle and action-entry blends.
- A focused `sim.m4_ssbm_falcon_wait_idle_lifecycle` test runs 440 production
  ticks and 38 stored poses. `tools/verify_ssbm_falcon_wait_lifecycle.sh`
  provides one at-will fresh-capture/source/generated-data qualification route.
  The importer skill now records DAT offsets, RNG isolation/injection, rejection
  semantics, transactional state, and terminal-plus-one blend ownership.
- The reviewed deterministic replay corpus/final/event SHA-256 values are
  `469c03272c7ce71f684bad27dd53f55d76a4ace72535152ed2fa5cc451a78315` /
  `c00595389591d404fd06e60780138a99dfda498a6160c7318b3b6acf713d3081` /
  `6f0f9376198d1f9507e6502da4eece00110a6ebe7c18233c78303d5b9764743d`.
- Native Windows MinGW Release passes 27/27 in 6.96 seconds; WSL Release
  passes 41/41 in 3.23 seconds; focused WSL ASan/UBSan passes combat plus the
  lifecycle oracle 2/2. The 21-domain / 117-case stored gate plus replay passes
  in 765.015 ms in WSL, and both standalone M2 kernel/replay verification
  scripts pass after restoring their shared HSD/fixed-math source closure.
  The prescribed MSVC gate is unavailable on this host because Visual Studio
  `vswhere.exe`/MSVC 14.44 is not installed; the local Emscripten SDK is also
  absent, so CI remains the web compile gate for this commit.

## 2026-08-11 ordinary-action collision ECB production routing

- Reused the paired 2,974-row ordinary hit/hurt capture rather than adding a
  duplicate browser probe or Dolphin scenario. A generic qualification key now
  selects the manifest-owned action subset, and report-only mode remains
  available for discovering callback/blend exceptions before asserting them.
- Twenty grounded routes reproduce 1,676 live observations / 1,450 unique
  frames across both independent captures with maximum one-Q16 coordinate
  error: Jab 1/2/3, Rapid Jab Loop/End, Dash Attack, five forward tilts,
  Up/Down Tilt, three real forward-smash angles, Up/Down Smash, and both grabs.
- Production maps the effective action through the existing imported move and
  uses its subaction index plus a generated per-motion displayed-frame offset.
  The shared allocation-free DAT/HSD evaluator remains the only geometry
  implementation; no runtime parser, float, duplicate action table, or
  rollback field was introduced.
- The parent-closed profile now contains 45 motions, 2,976 FObj tracks, and
  32,285 keys under decoded-data SHA-256
  `17da37dd9cdb080559407a7b8268bc52a590063bf9c84ef9b34e2de324e78dee`.
  The focused native HSD gate checks every one of the 725 represented runtime
  action frames and rejects unqualified actions.
- Rapid Jab Start remains open because its entry blend differs from raw HSD by
  up to 3,702 Q16 units. The five aerials remain open because callback and
  bottom-lock behavior differs by 29,000-70,000 Q16 units. They are explicitly
  excluded instead of routing known-wrong raw poses.
- Windows MinGW Release passes 39/39 in 4.94 seconds and WSL Release passes
  41/41 in 3.22 seconds. The full 21-domain / 117-case stored-plus-replay gate
  passes in 999.624 ms on Windows and 855.897 ms in WSL.

## 2026-08-11 common airborne ECB lock and aerial HSD routing

- A pinned/current decomp sweep identifies the shared owner rather than an
  aerial-specific callback: `ftCommon_8007D5D4` converts the fighter to air,
  stores ECB lock 10, and marks collision bottom locked. `Fighter_procMap`
  decrements before collision, so the entry collision observes lock 9 while
  retaining the previous desired bottom.
- All five aerial attacks now use the same allocation-free DAT/HSD evaluator
  as other qualified actions. Two existing independent Dolphin captures cover
  2,066 selected observations / 1,840 unique frames across 25 action routes;
  all qualified coordinates agree within one Q16 unit. Rapid Jab Start remains
  the sole action-ECB exception because its entry blend is not a raw HSD pose.
- The route-captured 195-value aerial bottom table is removed. One common
  transition detector initializes the ten-update lock for grounded-to-air and
  aerial-jump conversions, preserves an already locked desired bottom, and
  releases to the current HSD bottom. A production integration fixture checks
  the double-jump entry, Nair frames 1-8, and frame-9 unlock.
- The parent-closed profile grows to 50 motions, 3,366 tracks, and 37,366 keys
  under decoded-data SHA-256
  `caab1daafb4b54c836b1eee697ebe01935780561ed5ddaf421c3039ea4d7a552`.
  The deterministic replay corpus/final/event SHA-256 values are now
  `7de13f6a61f41619113c004203979d889a3603d2d8e5a60cd0ed2fab96d7a35f` /
  `7d031c271e05fb0041fa749488689175fb6b775f44d58a794bc1aa1e1c47bd48` /
  `55581ad6489814368e540e8eb96779ece01d840b1dd6ce7899afd1c4f724ac6bd`.
- Rebuilt Windows and WSL trees pass 41/41 tests. The complete 21-domain /
  117-case stored gate plus replay passes three isolated Windows runs in
  984.414-1,062.244 ms and three WSL runs in 1,294.885-1,754.153 ms, retaining
  the existing two-second budget.

## 2026-08-11 Rapid Jab Start ECB reference-space closure

- Rechecked pinned decomp revision `9509dc0`, current upstream `d882af9`, and
  Falcon's imported submotion records. Attack100Start has zero entry blend;
  the previous blend explanation was disproved.
- The exact residual was animated TransN. `ftAnim_8006E054` extracts and zeros
  TransN only when Fighter animation flag `0x80000000` is present. Since
  Attack100Start lacks that bit, `mpColl_LoadECB_JObj` observes model-root
  coordinates rather than TransN-relative coordinates.
- The shared production evaluator and all source verifiers now derive that
  reference-space decision from the already imported per-submotion flag. This
  adds no action branch, duplicate data table, runtime parser, allocation,
  floating point, or rollback state.
- Both independent 2,974-row captures now qualify all 26 ordinary action
  motions: 2,086 observations / 1,850 unique frames, maximum one-Q16 error.
  Rapid Jab Start itself is exact across all ten captured rows. The focused
  native primitive covers 925 poses.
- The parent-closed profile now contains 51 motions, 3,424 tracks, and 37,533
  keys under decoded-data SHA-256
  `2e1bec542d6c3ae6ce21f814039bab2b81caf05f2eac03b05ecd0d0118189bd2`.
- Repaired the shield-break verifier's stale multi-track assumption. It now
  verifies the complete four-track semantic digest and profile provenance,
  then compares the selected branch track against both qualified live
  captures. The full dynamic source gate passes 262 samples / 2,882 capsules
  at maximum two Q16 for hurt and one Q16 for ECB; its branch predicate remains
  `-3921` Q16 and repeat-stable.
- Rebuilt native Windows and WSL Release trees pass all 41 CTests. The complete
  21-domain / 117-case stored gate plus deterministic replay passes in
  1,120.723 ms on Windows and 1,486.147 ms in WSL, below its two-second budget.

## 2026-08-11 shield-break ECB source-closure repair

- Propagated the qualified current shield-break ECB profile/capture identities
  into the complete Falcon frame-data importer. The prior pins still named the
  older source capture even though the runtime semantic payload was unchanged.
- Two independent runs from all five pinned external inputs now regenerate the
  include byte-identically at SHA-256
  `e936f0edef8cdab44a6507d8b1c7f5474ea1950ead5a82a1f5f9d2a2e9478ebe`.
  The complete Falcon source digest is
  `1e26a7fcb73c506e7dd446119896f6df90bdea0bb244c178066b4f19f5b72946`;
  runtime numeric tables are unchanged.

## 2026-08-11 ordinary damage HSD pose routing

- Pinned/current `ftCo_Damage.c` analysis establishes the reusable selector:
  pre-launch ground/air state, knockback level, and collided hurtbox height
  choose Falcon raw submotions 165-179. Ground-to-air conversion retains that
  choice. The source enters displayed frame one immediately, freezes it during
  hitlag, and advances one frame per resumed update.
- Reused the existing full collision-memory probe. Independent 138-row
  captures have SHA-256
  `e34454e4f4cd7c3e02d46285820ce8210b9c002f6a32242577fba98aa9f0e437`
  and
  `24dc8291bcfe9ca8e470bda95e34e97242eb1138a5fc356eef91746777201401`.
  The source qualifier checks 276 DamageN2 pose observations / 3,036 capsules
  plus 132 post-entry ECB rows; maximum hurt and ECB error is one Q16 unit.
- Extended the shared manifest/verifier with repeated source-frame patterns,
  label exclusions, and capture-owned grounded/ECB-lock state. The six
  `_pre_hit` ECB rows remain explicitly mixed ownership: the damage skeleton
  has switched before the map callback replaces the preceding desired ECB.
- The existing parent-closed profile now has 66 motions, 4,381 tracks, and
  44,149 keys under decoded-data SHA-256
  `d013285272bfe3c4ad7a52218d24dbc7aabda24293289fbc06445fd51ae68109`.
  Production preserves the first accepted hurtbox height through collision
  reduction, selects the source motion from one constant table, and shares the
  allocation-free HSD evaluator with collision and inspection. No new rollback
  field, duplicate pose table, runtime parser, allocation, or float was added.
- The existing flat-ground knockback oracle now asserts DamageLw1 selection,
  hitlag clock freeze, resumed progression, every imported production hurt
  capsule, and mid-damage save/load. It also checks all 24 entries of the
  ground/air x knockback-level x hurtbox-height selection table.
- Windows MinGW Release passes 39/39; WSL Release passes 41/41. The complete
  21-domain / 117-case stored gate plus replay passes in 898.577 ms on Windows
  and 825.175 ms in WSL; focused WSL ASan/UBSan combat also passes.
  Deterministic replay corpus/final/event SHA-256 values are
  `7f210b0b70d2a506f60da411d4212885a5714ddc816c6fb076ad6273939a5ef0` /
  `7d031c271e05fb0041fa749488689175fb6b775f44d58a794bc1aa1e1c47bd48` /
  `55581ad6489814368e540e8eb96779ece01d840b1dd6ce7899afd1c4f724ac6bd`.
  DamageFlyTop/Roll selection, explicit hit-entry ECB callback ownership, and
  broader physical damage routes remain open.

## 2026-08-11 grounded slope damage launch and landing ownership

- Pinned/current `ftCo_Damage.c` agree on `ftCo_8008DCE0`: compare the
  pre-DI launch against the live floor normal, keep levels 0-2 grounded when
  the vector does not point away, and always launch level three while
  reflecting its vertical component only past 90 degrees plus common-data
  `x1E8`. Production imports `x1E8=10 degrees` and `x1EC=0.8` and performs the
  test in fixed-point Melee source coordinates.
- The production resolver preserves three distinct values: the original
  motion-selection vector, the projected/reflected physical `x8c` vector,
  and raw `xF0_ground_kb_vel`. This keeps DamageFlyTop/Roll selection before
  slope mutation and avoids an authored slope table or runtime float.
- The focused live theorem uses actual Falcon Forward Tilt inputs on Hyrule
  line 36. Mirrored attacks against a crouch-cancelled target produce one
  grounded projected DamageN2 route and one airborne route that recontacts
  into Landing on the first post-hitlag update. Both 60-row captures have
  identical observations and semantic SHA-256
  `657b816faa98658d10be6783b912a380cf88c24ccc1120d0a5836f61e6aa6ac9`;
  production digest is
  `15b3705d0c7a6e9c83d3a540c6b90da4af835676011a2726fdb360a3e8fdf05e`.
- The route exposed two shared callback-order gaps. Ground-origin airborne
  damage now installs the ten-update previous-bottom ECB lock before hitlag,
  and basic Landing preserves incoming air `x8c` on its entry frame before
  the next grounded callback projects it. The common lock adapter also stores
  root-space bottom rather than its inverse collision-sweep extent.
- The registered stored gate now covers 22 domains / 119 cases and passes in
  1,707.169 ms on native Windows and 1,035.323 ms in WSL. Rebuilt Release
  suites pass 35/35 on both. Replay corpus/final/event SHA-256 values are
  `7f210b0b70d2a506f60da411d4212885a5714ddc816c6fb076ad6273939a5ef0` /
  `7d031c271e05fb0041fa749488689175fb6b775f44d58a794bc1aa1e1c47bd48` /
  `55581ad6489814368e5408eb96779ece01d840b1dd6ce7899afd1c4f724ac6bd`.

## 2026-08-11 floor-response knockback channel ownership

- Pinned decomp `9509dc0` and current upstream `acfb24e` agree that ordinary
  Landing, neutral tech, directional tech, and missed tech do not share one
  knockback-entry policy.
- Basic Landing and `PassiveStandF/B` preserve the complete incoming air
  `x8c_kb_vel` with `xF0_ground_kb_vel` zero on entry. `Passive` and
  `DownBound` call `ftCommon_8007CCE8`, initializing/clamping `xF0` and
  projecting `x8c` immediately. Ground decay begins on the next update.
- The existing 804-row actual-input Final Destination capture now explicitly
  qualifies the split: directional-tech frame one retains vertical knockback;
  neutral/missed-tech frame one is projected; every route is projected on
  frame two. Production also asserts the otherwise unexposed `xF0` channel.
- Updated floor/slope-damage/slope-ledge production digests are
  `47ebff88692b3344c5e2cf24e790763c572d78e687be17e3d78a09e8e875f04a`,
  `15b3705d0c7a6e9c83d3a540c6b90da4af835676011a2726fdb360a3e8fdf05e`,
  and `bf8b2f390b2246835678a49ce191120ac4b8f39a4fb82130e9df5675354ac8a4`.
  The 22-domain / 119-case plus replay gate passes on Windows and WSL under
  two seconds with unchanged replay corpus/final/event digests. Release suites
  pass 41/41 on both platforms, focused WSL ASan/UBSan combat passes, and the
  verifier soak digest is `f965394d7f9f082a` on both Windows and WSL with its
  match/event/rollback/replay counters unchanged.

## 2026-08-11 non-tumble damage floor selector and retained lifecycle

- Pinned decomp `9509dc0` and current upstream `d882af9` agree on
  `ftCo_Damage_Coll`: after floor contact, forced-down or isotropic `x8c`
  magnitude at least common-data `x1E0` enters DownBound, magnitude at least
  `x1E4` enters basic Landing, and a smaller vector changes only kinetic state
  while retaining the current Damage action.
- The common-data importer now owns exact raw words `x1E0=0x40a00000` (5.0)
  and `x1E4=0x3f000000` (0.5). Production undoes the simulation's anisotropic
  scaling and compares squared Q16 magnitudes, preserving inclusive source
  boundaries without `sqrt`, host floating point, allocation, duplicated
  tables, or new rollback state.
- Reference-data Damage actions no longer terminate merely because hitstun
  reached zero or a below-0.5 route touched down. The selected Falcon source
  submotion remains authoritative through its imported terminal frame, while
  ground physics updates velocity ownership independently. Authored fighter
  behavior retains its previous release policy.
- The existing Hyrule line-36 pack now includes two actual-input low-speed Jab
  routes. The terminal route lands while remaining DamageN2 on sample 16,
  continues through sample 24, and enters Wait on sample 25. The IASA route
  presses B on released grounded Damage frame 15 and enters Falcon Punch on
  sample 17, proving the source `ftCo_Damage_IASA` to `ftCo_Wait_IASA`
  callback path. Two fresh 120-row captures pass with source semantic SHA-256
  `2ad67d79ef1fa278e5ea55096b663b0e59793167161eedc870e4c7663fe7a6a5`;
  production SHA-256 is
  `cb0b203a0a211baa55b800cd9e0cf0eb8e4595eaa069c8e865369cad8c94de61`.
  Their warm durations were 2.294709 and 2.171371 seconds.
- The registered gate now covers 22 domains / 121 cases. It passes with replay
  in 1,142.906 ms on native Windows and 894.651 ms in WSL, retaining the
  two-second budget. Rebuilt Release suites pass 41/41 on native Windows and
  WSL; the full 20,000-tick WSL ASan/UBSan combat trace also passes.

## 2026-08-11 common special acquisition masks

- Pinned decomp `9509dc0` and current upstream `d882af9` agree on the common
  IASA callback lists. `SquatWait` and `SquatRv` expose only `SpecialLw`;
  `Turn` exposes `SpecialS`, `SpecialLw`, and `SpecialHi`, but no `SpecialN`.
- Production now represents those callback lists as a four-bit stack-local
  capability mask rather than a coarse action boolean. Direction priority is
  evaluated after masking absent callbacks, which preserves diagonal down-B
  from crouch wait/end while rejecting neutral-B during Turn. No parser,
  allocation, duplicated state table, or rollback byte was added.
- The first live run revealed that a recorded input shared the same EXI frame
  as checkpoint restoration and could therefore observe or mutate the branch
  being discarded. Checkpoint protocol v2 now owns sixteen fixed immutable
  slots and consumes each requested load on an unrecorded neutral boundary
  before sending the case's first input. Eight one-shot Wait snapshots keep
  divergent specials independent inside one Dolphin process.
- Two fresh headless/null/unlimited captures are byte-identical at raw SHA-256
  `eed4476d41a97641c48114b453985f0306ab55785aeb49c4be8869e1fc51f5e5`.
  They cover twelve cases / 160 rows in 1.693055 and 1.447210 seconds warm:
  Turn neutral/side/up/down and SquatWait/SquatRv straight/diagonal down-B.
  Source semantic SHA-256 is
  `92ed40ead35b06ae754f1289a585ca0744fa3864adf5a25ed2d4f06278a09867`;
  matched production SHA-256 is
  `92ed40ead35b06ae754f1289a585ca0744fa3864adf5a25ed2d4f06278a09867`.
- The shared natural-movement projector now accepts declarative action-state
  and displayed-frame offsets for character action aliases, allowing Falcon
  Dive/Falcon Kick source names to reuse the generic native-CSV runner. The
  registered gate is 23 domains / 129 cases plus replay. The focused domain
  passes in 359.829 ms on Windows and 462.900 ms in WSL; the complete gate
  passes in 1,190.123/987.654 ms. Windows and WSL Release pass 41/41, and
  focused WSL ASan/UBSan movement/combat pass.

## 2026-08-11 teeter special acquisition

- Pinned decomp `9509dc0` and current upstream `d882af9` agree that
  `ftCo_Ottotto_IASA` owns all four common special callbacks and
  `ftCo_OttottoWait_IASA` delegates directly to it. Production's single
  TEETER action now exposes that same full capability mask for both phases.
- The source pack reaches real `EDGE_TEETERING_START` through an unrecorded
  low-stick walk and neutral release near Final Destination's endpoint. The
  native runner reaches TEETER through a velocity-aware ordinary-walk pre-roll
  on a tiny inert floor. Neither side mutates the fighter into the state under
  test.
- Four immutable checkpoint cases qualify neutral, side, up, and down B across
  28 retained rows. Two fresh headless/null/unlimited captures are
  byte-identical at raw SHA-256
  `a21b615da3f45642278ce4a1b2f6ba8335588e2568e423b2467fd1d55119bcca`.
  Source semantic SHA-256 is
  `2065e789ba0285f8b3d878bdc2615bf0a7e983ee02da356f6f46d0b924a6908e`;
  matching production SHA-256 is
  `2065e789ba0285f8b3d878bdc2615bf0a7e983ee02da356f6f46d0b924a6908e`.
  Warm captures take 0.268028 and 0.245186 seconds.
- The generic stored registry now covers 24 domains / 133 cases plus replay.
  The complete gate passes in 1,124.230 ms on Windows and 1,271.197 ms in WSL.
  Windows Release passes 41/41, the WSL Release configuration passes all 35
  enabled tests, and focused WSL ASan/UBSan movement/combat pass.

## 2026-08-11 GuardOff callback acquisition

- Pinned decomp `9509dc0` and current upstream `d882af9` agree that
  `ftCo_GuardOff_IASA` exposes the common special/attack/grab dispatcher only
  while GuardOff's powershield work flag is set. It then checks spot dodge and
  button/tap/C-stick jump on both ordinary and powershield release.
- Production reuses the allocation-free four-bit capability mask and existing
  powershield release-cancel predicate. ShieldRelease conditionally gains the
  four special bits without a new table, parser field, allocation, rollback
  field, or duplicated special router.
- A 17-case physical pack uses Falcon Jab 1 to qualify neutral/side/up/down B,
  jab, forward/up/down tilt, forward/up/down smash, grab, an ordinary-shield
  neutral-B negative control, and ordinary/powershield jump plus spot dodge.
  Its generic opponent-owned conditional edge reaffirms shield at the observed
  Jab frame after shield begins on the causal attack edge, while symmetric
  full position-history resets prevent restored attacker collision state from
  shifting contact. EXI batching is disabled for this exact input/collision
  route. Six concurrent workers retain no more than three divergent physical
  cases each, avoiding the observed long-session rollback boundary.
- Two 119-row headless/null/unlimited captures are byte-identical at raw
  SHA-256
  `20e3d7a2e5e5cba93df059069b72cf560a0c4641258582997fd6aebc6bdc8649`.
  Source/production semantic SHA-256 values are
  `851a0c05e393bd644344bf8a49d70fceea179727903ff68feacebb1c12a27c0d` /
  `851a0c05e393bd644344bf8a49d70fceea179727903ff68feacebb1c12a27c0d`;
  warm captures take 7.867078 and 7.607262 seconds, while complete cold runs
  take 9.102883 and 7.937196 seconds.
- The registry now covers 25 domains / 154 cases plus replay. The complete
  gate passes in 1,138.696 ms on Windows and 923.125 ms in WSL; the focused
  17-case lane passes in 879.898/417.053 ms. Both post-rename Release suites
  pass 41/41; focused WSL ASan/UBSan movement/combat passed for the production
  implementation.

## 2026-08-11: callback-priority fidelity and exact live/native equality

- Audited pinned decomp `9509dc0` and current upstream `d882af9` before the
  implementation. `ftCo_Walk_IASA` checks Catch before Special, whereas
  `ftCo_Wait_IASA` checks Special before Catch and attacks. Falcon's reference
  dispatcher now receives those raw chords before the authored charge,
  reflector, and projectile frontends can consume them.
- Extended the checkpoint acquisition pack to twelve cases / 160 rows with
  actual Walk and Wait entries. Walk Z+B selects Grab, low-axis Walk B selects
  Falcon Punch, Wait Z+B selects Falcon Punch, and Wait up+A+B selects Falcon
  Dive. Two live captures are byte-identical at raw SHA-256
  `eed4476d41a97641c48114b453985f0306ab55785aeb49c4be8869e1fc51f5e5`.
- Extracted the native CSV execution/canonicalization into
  `ssbm_native_csv_trace.py` and added a generic direct comparator. The first
  direct comparison exposed four previously pinned divergences: Turn-special
  facing, Teeter up-special eligibility, Falcon Punch's `ft_800827A0` mode-2
  endpoint clamp, and GuardOff EscapeN/attack entry clocks. All are closed.
- Common acquisition, Teeter acquisition, and GuardOff acquisition now have
  byte-for-byte equal canonical source/production payloads with SHA-256 values
  `92ed40ead35b06ae754f1289a585ca0744fa3864adf5a25ed2d4f06278a09867`,
  `2065e789ba0285f8b3d878bdc2615bf0a7e983ee02da356f6f46d0b924a6908e`,
  and `851a0c05e393bd644344bf8a49d70fceea179727903ff68feacebb1c12a27c0d`.
  Their manifests require exact source equality, preventing a divergent
  production digest from being relabeled as equivalence.
- The complete 25-domain / 154-case stored registry plus replay passes in
  1.129 seconds on Windows and 1.210 seconds in WSL. Direct live/native
  comparison passes for all three upgraded domains on both platforms. The
  final Windows and WSL Release suites pass 41/41; WSL ASan/UBSan passes the
  movement, combat, projectile, reflector, and charge lanes. The corrected
  match-soak digest `c14457eb6af305b9` repeats identically across three
  Windows and three WSL runs before repinning.

## 2026-08-11: initial-Dash callback fidelity

- Continued the pinned/current decomp callback audit through
  `ftCo_Dash_IASA`. Unlike Run, Squat, Landing, Walk, and Wait, early Dash
  exposes only SpecialS through common-data boundary x4C. Production had
  incorrectly enabled all four Falcon specials there.
- Filled the remaining four immutable checkpoint slots with natural Dash
  side/neutral/up/down-B cases. Side-B enters Raptor Boost, neutral/down-B
  retain Dash, and rejected up-B falls through to the running tap-jump
  callback, producing KneeBend frames 1-4 and JumpF frames 1-2. This also
  exposed and removed a production-only early-Dash tap-jump block.
- Two 188-row captures are byte-identical at raw SHA-256
  `f92c89a2108d880746bf66d286d42dfcfcb5ad87eee425dec22cdf933115e4cc`.
  Their complete canonical source/native payloads match on Windows and WSL at
  SHA-256
  `8fbfbcb12c5cdb483891315a4dc4c57a642c28ae2eb8ad886b31fecf9d3cd03d`.
  The final 25-domain / 158-case stored lane passes in 1.214 seconds on Windows
  and 1.771 seconds in WSL; both full Release suites pass 41/41 and focused
  WSL ASan/UBSan movement passes. The
  changed early-Dash route advances the deterministic replay corpus/final/event
  SHA-256 values to `1b2d49314b692a03114396f7eb662b5b574a1a2e0b045b9fa0a366db12852301`,
  `d9552577f2a31dcbcf582045cfc5af4033c15b519d4d315271e79e74a177c2af`,
  and `7930e2a2d90ed4dd9f5234ba47f4d4fc11e2ce4fbc2cd22b9367473a71bb2451`;
  each repeats identically three times on Windows and three times in WSL
  before repinning.

## 2026-08-12: aerial neutral-special turnaround qualification

- Audited pinned decomp `9509dc0` and current upstream `d882af9` through
  `ftCo_SpecialAir_CheckInput` and the global input-history update. Neutral
  aerial B reverses only when the remembered horizontal threshold crossing is
  opposite facing and its age is strictly below imported common-data `x224`
  (`20`). Production already used the imported field and rollback-safe
  direction/age state; the missing gap was executable qualification.
- Generalized the special-acquisition capture plan with compressed recorded
  pre-edge phases. The same natural jump, horizontal flick, neutral aging, and
  B edge now feed Dolphin and the generic native CSV runner; no action or input
  history is written directly and no character-specific native pre-roll was
  added.
- Three checkpoint-isolated controls prove opposite reversal at age 19, no
  reversal at age 20, and no reversal for a fresh same-direction age-19 flick.
  Two 91-row captures are byte-identical at raw SHA-256
  `3d4bb6c4a7cde8d2879e846eecf7e2fc3ca0d5151eb466fc7760678c83f58ad9`.
  Warm captures take 0.177852 and 0.192949 seconds; complete lifecycles take
  3.370682 and 3.479022 seconds.
- The reusable live verifier and direct comparator agree with production on
  every action, tick, facing, and grounded sample at canonical SHA-256
  `027fad335436a97393260b553019fe6247661b3ae1c03d981b4b1db4cc4d5fcb`.
  The complete 26-domain / 161-case stored registry plus replay passes in
  1.191 seconds on Windows and 1.065 seconds in WSL. Direct live/native
  comparison passes on both platforms, both full Release suites pass 41/41,
  and focused WSL ASan/UBSan movement passes.

## 2026-08-12: KneeBend up-special callback fidelity

- Audited pinned decomp `9509dc0` and current upstream `d882af9` through
  `ftCo_KneeBend_IASA`. The symbol `ftCo_Attack100_CheckInput` is misleading:
  its body dispatches `ftData_SpecialHi` when `x686 == 0`. KneeBend therefore
  checks up special before Catch and UpSmash, then evaluates short hop.
- Added `PF_M4_REFERENCE_SPECIAL_UP` to KneeBend's existing allocation-free
  callback capability table. No action-specific router, canonical field,
  snapshot byte, allocation, or duplicated special transition was added.
- Captured three naturally entered KneeBend cases: up-B, simultaneous up-B+Z,
  and rejected side-B. The two positive cases enter grounded Falcon Dive
  frames 1-6; the rejection remains KneeBend frames 2-4. Two 18-row captures
  are byte-identical at raw SHA-256
  `7523884c819b8ad371139b020cff562a0a0d4786cef1fde4a4dff2d499d42d51`.
  Source and Windows/WSL production traces are structurally identical at
  canonical SHA-256
  `0695488cb8bff660bfabe69298f366ed7bbbfed4348330636b04f87bff43aa17`.
- Generalized the shared acquisition verifier with declarative exact edge-row
  sequences and optional pre-edge action/frame checks. This represents
  rejected and mixed action trajectories without a Falcon-only verifier.
- The complete 27-domain / 164-case stored registry plus replay passes in
  1.614 seconds on Windows and 1.385 seconds in WSL. Direct live/native
  comparison passes on both platforms, both full Release suites pass 41/41,
  and focused WSL ASan/UBSan movement passes.
## 2026-08-12: crouch up-special callback fidelity

- `ftCo_SquatWait_IASA` and `ftCo_SquatRv_IASA` call down special, then the
  misleadingly named up-special dispatcher. Production now returns the
  existing up+down capability bits for Crouch/CrouchEnd instead of down only.
- Four natural axial/diagonal cases prove grounded Falcon Dive from both
  source phases. Two 106-row captures repeat byte-identically at raw SHA-256
  `527980419abfc7afdf7b698e65be21b0ed31e70a94a3443abd0a425da9ab29f4`;
  Windows and WSL production match source exactly at canonical SHA-256
  `3117c2767a723556602b43caf5b34cd9a0376f854adcd3f0f4f49d7c1c11bba6`.
- The complete 28-domain / 168-case registry plus replay passes in 1.293
  seconds on Windows and 0.979 seconds in WSL.

## 2026-08-12: released Damage versus DamageFall air-dodge callback

- Audited pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` through the actual callback bodies.
  `ftCo_Damage_IASA` delegates released ordinary airborne Damage to the normal
  Fall IASA table, which calls EscapeAir. `ftCo_DamageFall_IASA` owns the
  released DamageFly/DamageFall table and does not call EscapeAir. The pinned
  source-file SHA-256 values are
  `a3852f6377a71d03736b70b3869016a437b68c17dd703faead5be2954eb0278a`
  for `ftCo_Damage.c`,
  `973ce744a0e1084377bef6cebdeca6631fb90a0f8a31694621e0c5052b896a8b`
  for `ftCo_DamageFall.c`, and
  `cdff68de39d55855f1ca02b8e4af09ce856a1133cc21b23921a881b23e0dfaf6`
  for `ftCo_EscapeAir.c`.
- Production no longer treats every released damage action as EscapeAir-
  eligible. The existing digital-trigger transition requires the canonical
  tumble bit to be clear, preserving ordinary non-tumble Damage behavior while
  rejecting DamageFly/DamageFall. This reuses the shared air-dodge path and
  adds no content constant, action router, allocation, rollback field, or save
  byte.
- Two physical live cases retain 68 response rows. Released ordinary
  non-tumble Damage accepts a fresh L edge and enters `AIRDODGE`;
  DamageFly/DamageFall rejects the same edge and remains `TUMBLING` for the two
  retained airborne rows after it. The source semantic SHA-256 is
  `7ce52b784989e56f7539b79dd779eed94ab41e4bcd624b980c263af0b916084b`;
  production's canonical SHA-256 is
  `cec3d2b1d9b67ad906bf68b074c8975f6e53bf48cc714650c6498bef7aeba93e`.
  Capture A takes 0.587707 seconds warm. Capture B takes 0.897459 seconds warm
  and 4.981875 seconds for its complete launch-to-cleanup lifecycle.
- A generated two-case/four-sample numeric domain protects the discrete
  release result without replaying all 68 live rows in the fast lane. The
  complete registry is now 29 domains / 170 cases plus deterministic replay;
  it passes in 1.286 seconds on Windows and 1.188 seconds in WSL under manifest
  SHA-256
  `b4406686d48f9bcc8719d89f558a246dad05d25d5cb8362cde4b64d093aa0be2`.
  Windows serial CTest passes 40/40 in 8.42 seconds; WSL passes 42/42 in 9.82
  seconds.

## 2026-08-12: Run-to-RunBrake acquisition fidelity

- Pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` and current upstream
  `d882af94175e3c880ad51039e2979aa9a50aea09` establish the captured callback
  split: straight full down from ordinary Run enters RunBrake, a radial-gate
  diagonal-down edge remains Run, down from RunBrake enters CrouchStart, and
  the locked Run phase following TurnRun rejects direct down. The normalized
  source SHA-256 values are
  `72a9ce8c19948d468f6aea484b72db3b1f0c280846adc4d5677e4c6a20b810fe`
  for `ftCo_Run.c`,
  `0c75e6a95319f2be3a42dcade65b07671d47d7a31e7191e04cb617fce13866bb`
  for `ftCo_RunBrake.c`, and
  `80c2e71e50622e942754bfcdd3bd89f3762fe4df2400d8055f059ab6cc4b8082`
  for `ftCo_Squat.c`.
- Production excludes Run from the generic direct-crouch predicate and reuses
  the existing shared RunBrake transition plus the existing RunBrake-to-crouch
  path. No new state, lookup table, parser field, allocation, snapshot byte, or
  duplicate movement router was added. This supersedes the older direct
  Run-to-Crouch project behavior recorded above.
- Four immutable checkpoint cases retain 127 source rows. The independent
  captures reproduce identical ordered rows; their raw artifact SHA-256 values
  are
  `1d3c568f38f6dcd359e77c3b1616a6e7d81480dff4e8b3aa5262e528533fd8b9`
  and
  `e74a8c0ecc7628ba2886e7ad10b4633d2e1ad0eac5ecf6c5ec86f057a9d1ab16`.
  Warm work takes 0.541609/0.311504 seconds and complete lifecycles take
  6.177062/3.647950 seconds.
- The selected action/tick/facing/grounded source payload and native production
  payload are structurally identical at SHA-256
  `dfa7be0339110c98c9107a069ef7e9751b14f2c174bd04a7e977c90ae745f6ad`.
  The generated native-CSV domain keeps all four cases / 127 samples and passes
  in 263.089 ms on Windows and 403.007 ms in WSL.
- The registry now contains 30 domains / 174 cases plus deterministic replay.
  Three isolated complete Windows passes take
  1178.830/1319.197/1471.076 ms, and three isolated WSL passes take
  919.397/986.464/1270.306 ms under manifest SHA-256
  `99b5f633b2f4f6c33173ca285af0634e0ac51d1acc6df8b2a5b3c57f22cb261d`.

## 2026-08-12: native GameCube controls and result presentation

- Pinned SDL 3.4.12 defines GameCube face positions as A/X/B/Y for its
  south/east/west/north semantic buttons. The native playtest now branches on
  `SDL_GAMEPAD_TYPE_GAMECUBE`: A attacks, B selects special, X and Y jump, Z
  enters the existing attack-plus-shield grab chord, Start taunts, and the
  analog L/R axes remain pressure-sensitive shields. SDL's separate digital
  trigger-click buttons still saturate their matching shield. The Mayflash
  `0079:1843` DirectInput/raw-joystick fallback and its known button indices
  remain unchanged.
- The button translation is one shared switch used by live polling and a
  hardware-independent native smoke table. The table covers every corrected
  GameCube face/control route plus standard-layout strong attack and shoulder
  controls, preventing the type-specific branch from regressing ordinary
  gamepads.
- A terminated match now draws a centered result panel with the winning
  player/team (or draw); a time-limit truncation displays `TIME LIMIT` without
  inventing a terminal result. Both change the HUD phase to `RESULT` and give
  an explicit `R - REMATCH` cue. The existing deterministic reset is the
  rematch operation; presentation does not mutate simulation result state.
- Strict Windows MSVC `/W4 /WX` and WSL native targets build. Direct smoke and
  the focused `native_client.smoke` CTest pass on both platforms against SDL
  3.4.12. A real GameCube controller/adapter input pass and visual completed-
  match confirmation remain hands-on gates.

## 2026-08-12: early basic-Turn interrupt-facing fidelity

- Audited pinned decomp revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` and repository-current revision
  `d882af94175e3c880ad51039e2979aa9a50aea09` through the exact
  `ftCo_Turn_IASA` body. Both revisions have normalized `ftCo_Turn.c` SHA-256
  `3ad604c90ae3f67dd508cced55ab00ca6e7152a4a15693c5c78d4959434cbcfa`.
  The source temporarily exposes pending facing to side/down/up special,
  catch, smash, tilt, and jab callbacks before Turn's physical flip. If none
  consumes the transition, it restores old facing before guard, taunt, and
  jump.
- Production now supplies that source-scoped view through one allocation-free
  stack-local helper shared by its existing special, grab, and attack routers.
  Main-stick smashes are enabled on the same reference Turn callback surface,
  and both light-attack and smash direction selection use the temporary view.
  No canonical state, content field, parser branch, table, snapshot byte, or
  duplicated transition was added.
- Twelve checkpoint-isolated routes retain 72 live rows: jab, grab, all three
  main-stick smashes, all three tilts, all three C-stick smashes, and jump as
  the restored-facing negative control. Two captures repeat the same rows at
  raw SHA-256
  `294e9eae84ae6bc92f5932e20d666479b4455e49bb9666a54052003ec94b2c59`.
  The selected action/action-tick/facing/grounded source and Windows/WSL native
  payloads are exactly equal at SHA-256
  `0e9858e16140d8a55727255b009aec89e2176286e008b2cf23867bab38c2ac44`.
- The unchanged generic acquisition capture, comparator, generator, and
  native-CSV verifier own the fast gate. Its 12 cases / 72 samples generate
  artifact SHA-256
  `a0f269dd6683e372629190bfa011f35f79be226fc6a2a150c49eebc55961b8a0`.
  Focused Windows runs take 304.855/307.646/347.483 ms; WSL takes
  376.784/415.305/458.985 ms. The complete 31-domain / 186-case registry plus
  replay passes three isolated Windows runs in 1485.200/1706.950/1922.805 ms
  and three warm WSL runs in 1096.947/1127.462/1220.305 ms under manifest
  SHA-256
  `ffb5f801f55e24ce9c7a94fcbc627e46b1bcf0a42ebb79db37d746a1c9938664`.
- Deterministic replay was requalified at the first causal divergence, not
  blindly repinned. Checkpoints through completed tick 217 remain identical.
  On zero-based input tick 217, Player 3 is in early basic Turn and presses A
  with forward stick: the stale implementation entered jab facing left, while
  the corrected callback view enters forward tilt facing right at completed
  tick 218. Windows and WSL agree on new corpus/final/event SHA-256 values
  `6727023fb07bcb7a4fcbaf9c0beac0f8220c1c1802b19da891ae2ae2be252240`,
  `de96572115c1e4850d79353839576efc4b780ccbd75e8e70a2f23bee419c14af`, and
  `124a94734029321020513ec749b2f4d26cd60b4ed2129e25ce104692739fa9af`.

## 2026-08-12: duplicate web-probe retirement

- Removed four scripts that inspected source strings or repeated mechanics
  already owned by strict simulation tests: browser adapter, collision overlay,
  match flow, and replay visualization. Their checks could pass without
  exercising the compiled page and therefore were not independent fidelity
  evidence.
- Retained the non-duplicated bridge boundary in one compact CTest. It now
  verifies exact nonzero main/C-stick, button, and independent L/R-trigger
  forwarding; Team Lab's controller-two to simulation-slot-two mapping; and
  dynamic shield and multi-sphere hitbox packing through the production view
  packer. Test-only observation seams compile only under `PF_WEB_M4_TEST`; the
  Emscripten product carries no test hook.
- `browser-runtime` remains the generated-page/Chrome initialization gate.
  Collision toggling, setup-to-result/rematch transitions, and replay event
  navigation/fail-closed import now have separate deferred owner-interaction
  names. A static DOM dump no longer overstates those behaviors as exercised.
- Strict Windows and WSL `web.m4_playtest` CTests pass, the production
  Emscripten page rebuilds, and Chrome smoke passes with the current 83-event
  replay and final SHA-256
  `de96572115c1e4850d79353839576efc4b780ccbd75e8e70a2f23bee419c14af`.

## 2026-08-12: fictional Moonwalk-state retirement and source input-age correction

- The source audit found no Moonwalk action or authored setup duration in the
  NTSC 1.02 action tables or common movement callbacks. Moonwalk is an emergent
  composition of ordinary signed horizontal input history, Turn/Dash,
  retained velocity, and traction. The former two-tick setup, forced reverse
  velocity, and technique-only state/probe were modeling inventions and are
  now explicitly superseded wherever they had been presented as current.
- The base decomp increments a held same-side horizontal tilt age and saturates
  at 254, assigns age 0 on a fresh signed threshold crossing, and assigns 254
  inside the threshold. Dash checks the magnitude and age window, while every
  `ftCo_Dash_Enter` caller resets the age to 254. Production owns that reset at
  one shared Dash-entry boundary rather than duplicating caller logic.
- Public action values 71 and 72 remain reserved invalid holes. They are not
  renumbered away because later serialized values must remain stable; current
  and hitlag-resume validation reject them. State schema 76 retains the
  schema-75 1,607-byte payload and 1,747-byte format-66 checkpoint. Content
  schema 78/fighter schema 70 remove `moonwalk_setup_ticks`; inspection schema
  57 exposes the existing canonical age, and browser view schema 48 retires the
  two labels without growing its 603-value layout. Custom/reference packs must
  rebuild under the new content identity and cannot rely on the retired public
  actions, while unrelated custom behavior keeps its existing fields.
- The existing special-acquisition lane is the minimal live coverage route. A
  small optional input-memory probe reads fighter offset `x670`, and per-case
  `serialized_fields` keep that observation limited to relevant Dash samples
  rather than coupling input history to the large surface/pose probe. Source,
  capture, shard merge, and generated projection must agree on the probe
  provenance before the result is accepted.
- The fidelity target is GALE01 NTSC 1.02 with pinned UCF 0.84 enabled. Vanilla
  decomp remains the base source, but ordinary vanilla Turn timing is not the
  complete target: live qualification must record the exact active UCF code
  and hook inventory plus raw and processed stick history at modifier-sensitive
  boundaries. Historical authored `moonwalk_probe` results remain in the
  milestone record, clearly labeled superseded; they are not current evidence.
- No dedicated Moonwalk simulation test is added. Primitive input-age,
  Dash-entry, Turn/Dash, physics, save/load, and live-acquisition checks own the
  deterministic contract, followed by a GameCube-controller owner recipe for
  the emergent composition.

## 2026-08-12: pinned UCF 0.84 raw-input qualification slice (current)

- The executable target is now stated without ambiguity: owner-supplied
  `GALE01` NTSC-U revision 2 with official pinned UCF 0.84 enabled. Vanilla
  decomp remains the base callback/data authority, while modifier-sensitive
  behavior also requires the pinned UCF hooks and their raw controller history.
- The fixed-size input path now carries the physically serialized signed raw
  stick samples plus per-axis validity alongside processed Q15 input. Missing
  raw axes use the deterministic processed-input fallback instead of pretending
  that an unsupported replay recorded exact controller history. Canonical state
  retains the processed/raw history and UCF ages consumed by the hooks.
- All eight targeted UCF hooks are implemented and source-audited: PAD/cardinal
  preprocessing, Dashback, DBOOC, SDI, shield SDI, tumble, shield-drop
  suppression, and the extended pad counter. The current live pack directly
  qualifies the raw-history/Dashback boundary and delayed-Turn primitives;
  direct live boundary domains for the other seven hooks remain outstanding.
- The common-special acquisition pack now contains 19 cases / 210 rows. Two
  independent raw captures hash to
  `e4fde0c6b24f62f49a2af1d7e4a0e57d74c4b7750e36a6a3c73f43948789fe02`
  and
  `49d70c676de3d5a4d2071f8c0d9c6abba6c0feb7109fd73df9b3faa10b4c667d`.
  Their canonical source projection is exactly equal to production at SHA-256
  `6b50b9b36d47fb6a4b77bef5a951f03b311898fd88b481ff798909e05749f079`.
- The staged stored registry now declares 31 domains / 189 cases. The current
  deterministic replay is 42,555 bytes at corpus/final/event SHA-256 values
  `a1d9c1d97a3f20bdb9c76094c39b856f731a1eb2c0cca64ac05dd28a6e121949`,
  `3bdbbbc5d7faa6c8fd077ebd47aaa061f738a3561aa4c66ae2bfe4f8455cda6a`,
  and
  `a4020969be032543b9b229c8801bde77581b9f7fe26a9fe8aca91527627b13ec`.
  The full 31-domain / 189-case registry passes under manifest SHA-256
  `34be35b31153031861cbe481cdd1d4e94dd158d079b09efee9561de3389e77aa`.
  The stored verifier now calls its two reusable generators in-process and caps
  nested worker pools instead of starting 31 Python interpreters. Three
  sequential Windows runs take 943.548/903.249/1,068.009 ms and three WSL runs
  take 1,319.901/1,038.275/1,163.897 ms. The repeated-match verifier independently
  repeats digest `e70fc9c6d825c4a2` on Windows and WSL after the canonical
  UCF input-history state became hash-visible.
- The pinned Emscripten build and real headless Chrome smoke pass the current
  ABI-5 replay projection, including final SHA-256
  `3bdbbbc5d7faa6c8fd077ebd47aaa061f738a3561aa4c66ae2bfe4f8455cda6a`
  and all 84 re-simulated typed events.
- The Slippi differential parser and runner have 16 focused unit tests. The
  tracked legacy corpus remains correctly fail-closed: its old pre-frame
  payloads do not contain raw main Y and do not independently prove the exact
  disc/UCF revision, so they remain diagnostic rather than promoted evidence.

## 2026-08-12: pinned UCF 0.84 PAD/cardinal live/stored qualification

- Added a separate 16-case / 48-row physical domain for the modifier's main-
  and C-stick cardinal preprocessing. It covers each signed cardinal axis,
  raw dominant-axis 80 with orthogonal magnitude 6, and the discriminating
  dominant-axis 79 / orthogonal-magnitude 7 controls. The PAD/cardinal hook was
  live-qualified at this checkpoint; the six then-open boundary families are
  closed by the later DBOOC/shield and damage-input entries.
- The two independent capture SHA-256 values are
  `d6d7cb26d0b30785bb38c39a6b400366742998d6f9f2eeb448f4a7cb31db4984`
  and
  `b46ef4c579a26050f6cb8f9eda6c6c5068dd5b62ac65eae4ccdc0d9847075372`;
  their canonical 48-row projections are identical.
  Slippi's serialized raw main- and C-stick bytes are authoritative for the
  current physical sample. DME reads the source-owned fighter processed axes
  to prove the post-UCF result; the browser and ordinary processed-input
  fallback paths are not promoted as exact raw-controller evidence.
- The shared live projector and `native-csv-trace-v1` stored runner produce
  structurally identical source and production payloads at SHA-256
  `4a553ba57522d4347188cb227357157fbb4f1a7246dd638fba68019e9166fd63`.
  `falcon-common-ucf084-cardinal-input` adds 16 cases to the generic registry,
  bringing it to 32 domains / 205 cases. Direct verifier-only execution takes
  232.529-251.415 ms on Windows and 453.341-678.188 ms in WSL; the preceding
  31-domain / 189-case full-registry measurements above remain historical
  evidence for that smaller registry.
- The two four-worker captures take 7.349 and 6.883 seconds. Parent warm time
  is 8.097/7.357 seconds and cold time is 9.728/8.722 seconds, including setup
  and process overhead, so the manifest uses explicit 12-second warm and
  20-second cold budgets.
- The expanded 32-domain / 205-case registry passes under manifest SHA-256
  `26b925f08337c64e8cb8db9c5de7e47488d92b2fc3a6dd887894f53cbd095647`;
  three isolated complete runs take 877.821-1,033.078 ms on Windows and
  908.291-971.404 ms in WSL.

## 2026-08-12: common animation-before-IASA callback ownership

- Pinned `doldecomp/melee` revision
  `9509dc04406fb2028bfab01243841ba4787c0fb7` establishes the process order:
  animation runs before IASA, and terminal `Fighter_ChangeMotionState` calls
  replace the input callback for that same update. The audited common families
  are Squat/SquatRv, Landing and LandingAir, RunBrake/Turn/TurnRun/Dash,
  Appeal, GuardSetOff/GuardOff, EscapeF/B/N, ordinary attacks, and
  AttackAir-to-Fall.
- Production now derives one stack-local effective callback owner before every
  input router. The abstraction adds no allocation, canonical state, snapshot
  byte, replay field, or duplicated action table. A single entry-effects block
  retires source dash, attack, rapid-jab, and shield-stun bookkeeping.
- Focused movement coverage presses input on the terminal update itself and
  proves SquatWait Dash/Pass ownership, Wait's direct EscapeN and action table,
  TurnRun's Run/Wait split, GuardSetOff's immediate Guard callback, Down-Tilt's
  SquatWait target, and AttackAir's immediate Fall callbacks. Combat coverage
  proves terminal GuardSetOff roll/spot/jump buffering and powershield cleanup.
- The replay repin is causal: checkpoints 0 through 62 match the pre-projection
  build. Processing zero-based input tick 62 finds P1 on terminal Forward-Air
  Landing frame 18 with the fixture's explicit full-right segment held; Wait
  therefore enters Walk immediately instead of emitting the old artificial
  Idle row. Windows and WSL agree on the 42,555-byte corpus/final/event hashes
  `649b9ab2540b5e8d38b972756925b3349e82209235ed1aa8c58c8f51485ce1be`,
  `e4834ffac8b7be8ce77cf604710ca307caea512cca5eb00fae8487ca0fdc75b4`,
  and `787d63c5edf270cdc72d93dbe857c487bdc1ab7bdde59a1975299f1973fa7256`.
  The real Emscripten/Chrome smoke passes with 82 typed events; the removed two
  events were artificial intermediate transitions.

## 2026-08-12: UCF 0.84 DBOOC and shield-input boundary domain

- A new seven-case / 137-row Battlefield checkpoint pack covers DBOOC radial
  over/under and age control, shield-drop suppression at raw Y -63/-64, and
  extended pad-buffer delta 50 versus 44. No gameplay-specific runner or web
  probe was added; the route reuses `native-csv-trace-v1` and extends generic
  special acquisition with a naturally settled `platform_guard` owner.
- The capture wrapper can compose its existing schema-2 input and surface
  probes for checkpoint acquisition. Both probe descriptors are merge
  invariants, every platform case proves source floor line 2 immediately before
  the edge, and trigger-valued guard phases fail closed if omitted.
- Independent raw captures SHA-256
  `ebe6fe613e4e691adc8fec8f168ade7025ae1898544de1e084c2e59f42475874`
  and `493fe81f07dbcfeeb1674d959f3ed9db1416062e52450f33b379fe1928c58812`
  canonicalize identically. Source and production are byte-exact at
  `73198f0ee5ab242d72598c4fa149d6f13e60112d69ddeb1d1f83e0218683c009`.
- Full captures take 8.632-10.983 seconds; warm parent time is 8.048-9.618
  seconds. The focused stored gate takes 136.530-181.125 ms on Windows and
  384.845 ms in WSL. The complete 33-domain / 212-case registry passes under
  manifest SHA-256
  `d983e7855a31f696e87d00da97ea6e8ae28eefa0c4b353de61c4b68ad2aee788`
  in 588.997-719.748 ms on Windows and 779.541-1,095.532 ms in WSL.

## 2026-08-12: UCF 0.84 SDI, shield-SDI, and tumble boundary domains

- Added two compact, reusable damage-input domains rather than a new capture
  protocol. `falcon-common-ucf084-hitlag-input` has four cases / 16 rows for
  ordinary SDI and shield SDI; `falcon-common-ucf084-tumble-input` has two
  cases / six rows for DamageFall. Both reuse checkpoint v2, schema-2 input
  memory, exact raw-main native CSV rows, the generic trace generator, and the
  existing surface/damage capture routes.
- The live theorem proves the UCF strict boundaries themselves: raw delta 62
  rejects and 63 accepts SDI and shield SDI, while raw X delta 75 rejects and
  76 accepts the tumble wiggle. Ordinary/UCF ages, current and previous
  processed input, raw t-versus-t-2 history, hitlag/tumble, action, and
  grounding are retained. The hitlag comparator allows exactly one Q16 unit
  of measured source/native displacement rounding and no state/timing slack.
- Independent hitlag capture SHA-256 values are
  `fb636ab13fd6ecdcb8f10f11af3640b8fbf6759a18f7e5ce01e9a7904f581b6e`
  and `612a9eb7e72adb7b4119246242ff511613be40eb4d12a83b112e8aa0a7a0b38b`;
  source/production semantic hashes are
  `9f30698ba7ec1aafc5dd1bbb15e1a6f8bc1f503d04a4e86a318e74a5be3a87e4`
  and `7754c6342a567433d4fb4989405c9e309429782aa52dc34e49f76133b0f01303`.
  Independent tumble captures hash to
  `3737ce0d2006f6d64d44e498499877844c3d0a938d0be42c53818f355b287bd0`
  and `473881a96ad47339e2e22f74eefdf1b63930ea2f98cf45d6f7901e312131d84f`;
  source and production are byte-exact at
  `da5473c7bfd0883a405eef293d11eca8f7618f78999e42f73097aff99760ff00`.
- Primary hitlag/tumble captures report 7.046/4.459-second warm and
  14.411/15.298-second cold totals. Eleven focused input-trace tests and both
  stored CTests pass on MSVC and GCC. The expanded 35-domain / 218-case
  registry passes under manifest SHA-256
  `256cc0d55e882b5bff3a0dc52dc521db0d2e64ebb08ea9a479ee22ba81130946`
  in 619.867-788.424 ms on Windows and 947.881-965.814 ms in WSL.
- These domains complete direct live boundary qualification for all eight
  Falcon-relevant pinned-UCF hooks. The Zelda grounded-Up-B cardinal exception
  remains an explicit future character-import boundary, not Falcon evidence.

## 2026-08-12: Run/late-Dash GuardOn CatchDash provenance

- Imported PlCo common offset `0x68` as the three-update GuardOn dash-grab
  window. Run and late InitialDash entry initialize it; Wait and other Guard
  origins clear it. One shared GuardOn grab branch selects DashGrab while the
  window is nonzero and decrements it after each rejected GuardOn update.
- Added three cases to the existing common special-acquisition pack: Run
  positive, Wait-origin ordinary-grab control, and Run-origin expiry control.
  Two final headless captures both hash to
  `0a3c853d039fb2b5552d195a040bbac5335aa1fe512f0260376f45c05d980027`;
  source and Windows/WSL production are byte-exact at
  `087c81e3dbdc2794bc5bef1bbd8af32e68e3ee2fb36dc606b8ee266d5c1f2e4a`.
- State schema 78/save format 68 serialize the four future-affecting window
  bytes in a 1,787-byte checkpoint. The 240-input replay is now 42,559 bytes at
  corpus/final/event SHA-256 `116fb683a6aa5ff1f63ccec3b082fdce351eb985a1ef02214631ff7e80f36394`,
  `5a7db4a5e899b1af31909f7997dcb1a08226aec79f4f09fab7422fe9602f246f`,
  and `787d63c5edf270cdc72d93dbe857c487bdc1ab7bdde59a1975299f1973fa7256`.
- The complete reusable registry contains 35 domains / 221 cases; three
  sequential runs measure 774.267-843.944 ms on Windows and
  1,117.820-1,984.600 ms in WSL under manifest SHA-256
  `9ef8f2ea5f5a21a0f8856dcc42fb8b5aa313de1f38cac31d8ab6f2a744180670`.

## 2026-08-12: Walk reversal callback fallthrough

- Corrected the reference Walk movement tail without changing authored
  fighters. After the ordered Dash callback rejects an opposite tilt below
  the full-Dash threshold, `ft_8008A244` now enters Wait instead of the
  simulation inventing a basic StandingTurn. A fresh full reversal remains a
  smash Turn, preserving dash-dance input.
- Added two natural cases to the existing common special-acquisition pack.
  They retain Walk setup, edge, and one result row with exact source action,
  facing, grounding, and ordinary X-input age. Two independent 278-row
  captures both hash to
  `e4193dce5d782716f41d35b7495e94a345142412c0dbc4813aa430907a998a3a`.
- The expanded 24-case source and Windows production traces are byte-exact at
  `5a22a6f401df8a8557bd2ac16b5c3dd34211cf825f302a9f81db2f4e2897253f`.
  The focused stored gate passes in 250.781 ms under manifest SHA-256
  `ab99092abaf6231497ffc901499a11c0b6c29678bc70c7e60536fe1653a1b7c1`.
  Three complete runs take 887.531-946.419 ms on Windows and
  1,075.190-1,131.290 ms in WSL; strict CTest passes 47/47 and 41/41.

## 2026-08-12: InitialDash entry provenance and early callback split

- Pinned `ftCo_Dash.c` proves that `mv.co.dash.x4` distinguishes ordinary
  `Dash_CheckInput` entry from Turn-origin entry. Ordinary early Dash checks
  F-smash and held LR through PlCo x48 for EscapeF; Turn-origin Dash checks
  Dash Attack and Guard. The shared tail accepts Appeal in every phase.
- Production packs this provenance into the magnitude of the existing signed
  dash-direction byte: magnitude 1 is ordinary entry, magnitude 2 is
  Turn-origin entry, and sign remains direction. No canonical field, save
  byte, replay byte, allocation, or new public action was added. Save/load
  coverage preserves the Turn-origin branch.
- The common-data importer now owns PlCo x48 as
  `initial_dash_forward_roll_end_frame=3`, alongside x44=4 and x4C=20. The
  authored early-shield rejection and early-taunt lockout are removed.
- Two independent 322-row UCF Dolphin captures hash to
  `750800f604c03baed6c74870b43b624957cb65a89a8570c1f905b608eb1021c3`
  and `fb59fd9809c4f805436cfa0e298e9b4b452b562e6a489fb542147393d112cb15`.
  They prove ordinary A -> FsmashMid, Turn-origin A -> DashAttack, ordinary
  shield -> RollForward, Turn-origin shield -> GuardReflect, and early Appeal
  -> TauntRight.
- The 29-case / 322-row source and production traces are byte-exact at
  `0ca93dea87caf0f2911a16806e7402bbff054a0278ad2029a925d23f742f2fe1`.
  GuardReflect's source animation frame `-1` is the only excluded clock; the
  action, facing, grounding, inputs, and surrounding Dash clocks remain strict.
  Generated-oracle SHA-256 is
  `4f83a1ace4ff284b9a24852882ebdb797e3fb144cec64efbd9760ccdc8edab5b`.
- The 35-domain / 228-case registry passes under manifest SHA-256
  `53b22294f20946ec42cc32eb89d72d2b32b3c19d050996f3fd1585d43dffc39f`.
  Isolated runs take 888.277-1,118.575 ms on Windows and
  1,068.334-1,205.796 ms in WSL; the focused domain takes 254.896 ms.
- The 240 inputs, final state, and event stream are unchanged. Imported x48
  changes only the replay content identity to
  `fb0f4e7251e70f7660801222b5b5a2627e9c45e1b56b7d5763035947cb553d1c`;
  final/event hashes remain `5a7db4a5e899b1af31909f7997dcb1a08226aec79f4f09fab7422fe9602f246f`
  and `787d63c5edf270cdc72d93dbe857c487bdc1ab7bdde59a1975299f1973fa7256`.

## 2026-08-12: InitialDash acquired-state motion hardening

- Widening the same two live captures from action/facing/grounding to velocity
  and relative position exposed two non-Q16 gaps: early Dash -> Appeal and
  Turn-origin Dash -> Guard retained excessive Dash speed despite selecting the
  correct action.
- Pinned `ftCo_Dash_IASA` falls through after both successful callbacks and
  applies PlCo x54 before Appeal/Guard physics. The importer now owns x54=0.75;
  production uses one shared reference stationary-ground-friction primitive
  for Guard, GuardOn/Off/SetOff, and Appeal while preserving authored content.
- The widened 29-case / 322-row source and production traces are byte-exact at
  `a6b654c96b35d73122adc4d6ad92cf7d4b1c0fe064ebf466012094397b31522b`.
  Only accumulated 1-2-Q16 RollForward/DashAttack X-position rows are excluded;
  their velocities and all other stored motion fields remain exact. Generated
  oracle SHA-256 is
  `ebcf57082b682c7d9e982d7c2e2d3bec1dd99cbde8428d58b55c1bb696635630`.
- The full 35-domain / 228-case Windows registry passes three isolated runs in
  891.754-1,043.521 ms under manifest SHA-256
  `3723c3ef7d08336bf65208f90cda02e5f49f6a0434e2fa0763bbe1eebce643c1`.
  Three independent WSL/GCC runs take 1,183.405-1,546.116 ms. The corrected
  eight-match rollback/replay verifier soak is compiler-identical at digest
  `9796fb602c19ced2`.

## 2026-08-12: complete Dash acquisition motion projection

- The existing 29-case / 322-row live pack now compares relative position and
  both velocity channels across UCF and delayed vanilla Turn-to-Dash, Dash
  side-special, and rejected neutral/up/down-special fallthrough, in addition
  to the five InitialDash provenance cases.
- Source and production canonicalize exactly at SHA-256
  `5e9a79db538b171c208737628a7667f97ffffc2915b46d4744876b30c7cfb51a`.
  Narrow masks remove only measured one-Q16 accumulated X samples and one
  one-Q16 up-fallthrough velocity sample. Generated oracle SHA-256 is
  `3fee9dc65808095e2014a1c42c1765176f6cfcc8cadd3f0ce7e2f6868a9a5b0b`.
- The complete 35-domain / 228-case registry passes three isolated Windows
  runs in 1,103.784-1,343.801 ms and three WSL runs in
  1,187.017-1,371.182 ms under manifest SHA-256
  `d9abddfb6774379efde9081842ed0bd0f10924ea5d03008a841b1b9deab79055`.

## 2026-08-12: Walk/Run four-direction special acquisition

- Audited the pinned callback bodies instead of relying on decomp names.
  `ftCo_Walk_IASA` checks Catch before Side/Up/Neutral/Down Special;
  `ftCo_Run_IASA` checks all four specials before CatchDash. Production's
  existing zero-cost capability masks and shared directional resolver already
  match those bodies, so this slice requires no gameplay change.
- Added three missing Walk cases and all four Run cases to the existing common
  special-acquisition pack. Each natural Run route retains 25 Dash/Run setup
  rows before its B edge; each Walk route retains its acquired Walk row. Exact
  assertions cover the complete ordered action/frame sequence, facing, and
  grounding rather than filtering for only the expected successor action.
- Two independent 467-row captures hash to
  `34f721aea3df67edaea7435a256cc70e27d54767a2ae9b61fe4f6e22bc749ceb`
  and `d9af7fbc86479d1e3a8f823ae77e3bf5f8510d98db0c27ddeaa774436897033f`.
  Their semantic trace and Windows/WSL production are byte-exact at
  `9546194d57f47eb320102b70be475111cead96c975e74af486201f3bf6d67cbb`;
  generated-oracle SHA-256 is
  `23e7a23d300585b93ffe7d877ac4273ab1e00ece4bf160b57ba660c7b5df618e`.
- Rebalanced the manifest into three 156/155/156-row workers after a valid
  two-way run exceeded the warm budget. Accepted runs take 12.410-14.011
  seconds warm and 12.827-15.579 seconds cold. The full 35-domain / 235-case
  registry passes three Windows runs in 919.758-1,031.223 ms and three WSL
  runs in 1,394.273-1,589.864 ms under manifest SHA-256
  `f8637c42927fe1df228d4225607cbadd9211eae7a60a075e11c60f92e869b938`.

## 2026-08-12: complete 782-replay Falcon diagnostic sweep and wavedash boundary

- Downloaded and SHA-256-manifested all 782 Falcon `.slp` files in the pinned
  MIT-ranked archive at dataset revision
  `11142d4b86d423716fdd2e9ca565de9bafc9d37e`. The ignored full manifest hashes
  to `f42d437ee166b07502a7af8316aa2ee0b473ea487802b1327ae673171493b477`.
- The eight-worker run completed in 777.694 seconds: 782 parsed replays, 328
  natural anchors, 111 executable diagnostics, 2,475 checked semantic frames,
  39 passes, 7 UCF dashback boundaries, and 72 deterministic candidates. The
  ignored report hashes to
  `154ae335f86fa91aa6bbc5fe0bb0a6908d594efd44f0d220bcdba27e238a6e8f`.
  Every result remains diagnostic because the public files do not independently
  prove the exact disc image and UCF revision.
- Two independent replay prefixes exposed terminal KneeBend callback ownership
  and two-stage L input. Pinned Melee source installs Jump before IASA on the
  takeoff update; the source analog trigger endpoint can precede the physical
  L click by one frame. Production now shares one ground-jump entry and routes
  the terminal update through the new airborne callback owner. The replay
  adapter caps unclicked analog at `65534` and reserves `65535` for the physical
  click, preserving the fresh EscapeAir edge.
- The minimized replay now matches through LandingFallSpecial and proceeds to
  a separate later 96-Q16 aerial horizontal-velocity residual. Eighteen focused
  Python tests and focused Windows/WSL movement tests cover the analog-to-click
  positive sequence plus the early-click-held negative control.

## 2026-08-12: 2,093-replay Falcon sweep, Jump input order, and Turn-origin Dash reversal

- Completed the required prior-art expansion with the MIT-licensed
  `erickfm/melee-ranked-replays` Falcon shards at dataset revision
  `11142d4b86d423716fdd2e9ca565de9bafc9d37e`. The two archives contain 782
  and 1,311 unique `.slp` files and hash to
  `e7906939235c1841d8abb2e8eb160a7ed84b0d0da35407b5b053b85c5b5f5acb` and
  `60bb7e5cae1e469bdf54d646b60be5a5a77d0942a6c3b6405ba1600b23b2fe87`.
  The 2,093-file / 7.08-GB extracted corpus is ignored; its hash-pinned manifest
  SHA-256 is
  `0e7b2b0805de1b09999b1f3104fb483cb2cbfbf2083864bb2d0d43cda3b6cd62`.
- The first full 2,093-file pass found 887 natural anchors, executed 740
  diagnostic prefixes / 13,441 semantic frames, and recorded 210 passes, 396
  unsupported source-modifier boundaries, and 134 deterministic candidates in
  1,265.375 seconds. Because public setup metadata says only generic `UCF`, the
  worker now fails closed when raw primary/orthogonal bytes meet UCF 0.84's
  inclusive 80/6 cardinal gate but the source processed pair is not snapped.
  Such prefixes are classified as `ucf084-cardinal-signature-mismatch`; they
  are not reported as production divergences.
- The largest initial cluster exposed a real animation/input ordering rule.
  Pinned `ftCo_KneeBend_Anim` installs Jump before the update's input callback,
  and Jump entry consumes the preceding processed horizontal input. Production
  now routes ground-jump momentum and the Falcon JumpF/JumpB submotion choice
  through canonical prior processed X, while double-jump entry retains current
  input. Normalized source SHA-256 values are
  `91249dcf7a0aa59277e8912bd8b5a82548262df66ef3426d6ed3d27cebdd6c12`
  for `ftCo_KneeBend.c` and
  `6905ee45c6f498a73971682b3d49cb1896e3134a0ef936dd49ed49bfde6f5605`
  for `ftCo_Jump.c`.
- Two six-row live cases independently prove neutral-history and held-history
  terminal Jump entry. The source reports Jump frame-1 horizontal velocity
  0.0 and 0.95 respectively. The enlarged common special-acquisition pack has
  39 cases / 491 rows. Its independent raw capture SHA-256 values are
  `c2f6deed8346eb1729a9ee3d8444b77d9426e8fd457fb2b931a4978dc783704c`
  and `97095e1ebdc2fc5c022feec9d9fc6635c94988526576bf9cc76c22b2753dbb85`;
  source and production semantic hashes are
  `c68d3bc9cd830283648f98b210502a471ca0724fc3b5197caea5b71aaa29a07b`
  and `dfdf53934818d7436fa02fc265e3febb129e97f34b1573d0b670cfc66075a6c6`.
  Their only difference is one accepted Q16 unit on held Jump frame 2. The
  generated oracle SHA-256 is
  `7dcb3fdaae5cd74acbb31e5c537299bd07728696857855c7acaab77936e6d0fb`.
- A second full pass after the Jump correction executed the same 740 prefixes
  over 15,511 frames, raised passes to 254, and reduced deterministic
  candidates to 58 in 1,102.245 seconds. Its ignored report hashes to
  `8792b8f0685912030a72baf0482433836df603971fb183d4103465ca8e46c1ad`.
  The original Jump exemplar is now a clean pass.
- The largest remaining cluster (16 prefixes) was Turn-origin Dash reversal.
  Pinned `ftCo_Dash_IASA` gives Turn-origin Dash `mv.co.dash.x4 == 0`, so it
  reaches the normal Dash input callback and may reverse during displayed Dash
  frames 1-3; ordinary Dash keeps the early x44 branch and its lockout.
  Production's existing origin bit now participates in the reversal predicate,
  with no added state or runtime allocation. A new 12-row natural theorem
  proves Turn frames 1-7, Dash frames 1-3, then immediate Turn frames 1-2 on
  the reverse edge. Normalized `ftCo_Dash.c` SHA-256 is
  `23fd2ad0af701c320fb24f6b5e7406971d7c31060b87916a20b242c076d10f7c`.
- Twenty focused replay-worker tests, the 491-row live verifier, stored-oracle
  generation check, and strict Windows/MSVC plus WSL/GCC movement suites pass.
  The root stored registry remains temporarily blocked by its authored
  240-tick replay interaction fixture: the corrected Jump-entry semantics
  invalidate that fixture's required hit/SDI/grounded-roll route. The focused
  source/native qualification is passing; the replay golden must be rebuilt
  as a real interaction scenario rather than weakening its coverage checks.
  Public replay results remain diagnostic until the exact disc and pinned UCF
  0.84 revision are independently proven.
