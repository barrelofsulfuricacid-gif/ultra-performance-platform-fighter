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
  authors a 90-tick grounded `TAUNT`; entry retains dash velocity, ordinary
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
- Grounded full up plus a fresh special edge must start or resume Arc
  Reservoir. Early shield release during store must preserve charge and regain
  the grounded router, holding shield through the boundary must enter ordinary
  shield, Attack must release scaled damage, and a hit during charge/store must
  clear the value.
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
- Opposite input during initial dash remains a dash-dance reversal. Opposite
  input after entering `RUN` must instead enter `RUN TURNAROUND`; neutral or
  sub-threshold run input enters `RUN BRAKE`.
- Repeated same-direction full-input edges separated by neutral releases remain
  initial-dash fox-trot bursts. Holding the direction reaches `RUN`, and using
  the reduced walk magnitude after release cannot restart the dash.
- A one-tick opposite full input followed by neutral or a ground action remains
  the pivot route. Holding the reversal continues initial dash, while waiting
  until `RUN` produces `RUN TURNAROUND` rather than an empty pivot.
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
- Additional stage geometry beyond the current raised-block test fixture.

## Remaining M4.2 and M4.3 work

- Character-specific move breadth, additional specials, broader recovery
  options, broader throw routes,
  broader per-action launch-angle data,
  prone-orientation-specific getup-roll timing, and journal producers for every
  remaining action.
- Repeated human matches.
- The mandatory owner combat playtest; the generated browser worksheet is
  ready, but only the owner can supply and approve its evidence.

## First-slice verification

- Release workflow: 22/22 tests.
- Address/undefined-behavior sanitizer workflow: 22/22 tests; leak discovery
  disabled only for the restricted workspace.
- Mechanical oracles: 334 movement invariants including Moonwalk timing,
  Teeter-cancel, Taunt-cancel, Stage-humping, and Scar-Jump routes and controls, and mid-action
  save/load, plus Vector Ascent data, consumption, restoration, and RL routes;
  958
  attack/reaction/shield/floor/surface
  invariants including data-defined pummels, crouch cancel, victim weight, and
  stale-move queue scaling/registration,
  complete directional ground normals, canonical smash charge, the
  five-direction light-aerial vocabulary, exact shield size/tilt/poke geometry,
  horizontal shield SDI/ASDI,
  ledge attack, and ledge-roll invulnerability plus 51
  combat-journal invariants,
  24 stock/respawn/result
  invariants plus 44 match-journal and 24 moving-revival invariants,
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
  `142117769ea04308848f89a8812ce97861c56a5342862dded8bf506096fc2809`,
  final SHA-256
  `931c3ccc547f92f6d9ae9dc1ea4c7428315a757b4c165565424b41a6f788ada4`,
  and event-journal SHA-256
  `7dac547f463ec6995207dc41d8fab3449113b79cd6179d4037e821a8dc63b18f`;
  local native/WebAssembly output is byte-identical and CI repeats it.
- Clean Chrome CI remains the generated-Wasm, canonical replay-inspector, and
  live-playtest DOM gate.

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
