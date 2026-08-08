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
plus explicit first-airborne-frame instant double jump, double-jump-cancel,
and double-jump-cancel-counter verification
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
  by a browser verifier. `collision-hitbox-overlay` is now an active acceptance
  check, while tolerant screenshot comparison remains the separate planned M7
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
- `m4-local-match-flow` is an active verifier acceptance check over setup,
  playing, results, rematch, and return-to-setup states. This temporary M4.3
  surface does not claim the broader M7 menu-navigation system.

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
- `m4-replay-visualization` is an active verifier acceptance row, and the clean
  browser smoke requires the file control, stable visualization semantics, and
  three re-simulated canonical events.

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
- Down from an unlocked run remains the sliding crouch-cancel route; jump and
  shield remain the other live dash cancels. Initial-dash shield and
  run-turnaround crouch input remain rejected.
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
