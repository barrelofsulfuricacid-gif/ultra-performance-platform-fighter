# M4 combat vertical-slice progress

**Status:** In progress; M4.1 movement/ledge core, light and strong M4.2 ground
attacks, hit-reaction layers, missed-tech floor recovery, dense shield, and
physical powershield cancel, solid stage geometry, and wall/ceiling tech
plus directional air dodge, helpless fall, wavedash/waveland, the first
light and strong production aerial routes, auto-cancel, visibly scored
L-cancel practice, SHFFL, grounded forward/backward rolls, and spot dodge
plus explicit first-airborne-frame instant double jump verification
plus configurable stocks, delayed respawn, invulnerability, sudden death,
results, rematch, the bounded rollback-safe typed event feed, and complete
  shield-break launch/down/stand/stun/recovery, the three-tick small-step
  forward-smash route, the hitlag-assisted same-platform drop cancel,
  reduced-down shield platform dropping, three-frame V-cancelling, and
  ordinary-input approach, spacing, mindgame, cross-up, juggling, and
  platform-sharking routes, plus two-pad browser polling implemented

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
  blast-zone stock loss/respawn, ledge catch, catch lockout, hang, release, ledge jump,
  and ledge climb.
- Deterministic one-fighter-per-ledge occupancy with stable lower-slot priority
  for simultaneous catches.
- A rollback-safe state-schema-17/save-format-16 contract that serializes every
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
- One hundred four movement/content invariants plus a 20,000-tick four-player
  canonical-state trace under the active `M4-MECHANICS` verifier entry.
- A live two-player browser adapter that advances the production simulation at
  fixed 60 Hz, draws its inspected stage/player state, and supports pause,
  single-step, reset/rematch, stock HUD, respawn countdown, and result overlays.
- Explicit full-magnitude dash/dash-dance keys and reduced-magnitude walk keys
  for both keyboard players, with the real binary jump-squat selection rule.
- A native and Wasm startup contract that refuses readiness unless walk,
  dash-dance reversal, short/full-hop apex, aerial landing/L-cancel timing,
  instant-double-jump timing and held-input rejection,
  strong-aerial 30/15-tick landing timing, real damage/hitlag,
  shield-break phase/mash/recovery, and reaction-input and stock/respawn
  invariants pass.

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
  Config/observation/identity schema 2, inspection schema 14, browser view
  schema 14, and RL schema 4 remain current. The canonical save is 611 bytes.
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
- Starting that back aerial immediately after takeoff produces active frames
  on the wrong side and whiffs. Repeating the descent timing while facing
  toward the defender blocks but leaves the attacker in front, providing the
  deterministic side/facing control.
- The native oracle saves after rear-side aerial startup and proves 48 equal
  future hashes through the physical shield block and landing. Browser
  readiness walks both players to clear legal space on the default stage, then
  repeats the rear block, early whiff, and front block with ordinary input.
- Registry row 8, Cross-up, advances from `planned` to `playable`; the future
  production grab fixture must add the shield-grab comparison before complete
  verification, and owner execution remains required.

## Delivered in the mindgame route

- The same visible reduced-stick approach cue now supports two tested reads.
  Against the jab-first response, the attacker brakes outside reach and strong
  counters recovery. Against held shield, the attacker faces away, short hops
  through, and finishes the rear-side aerial cross-up behind the defender.
- The wrong branches remain ordinary outcomes: strong attack into shield is
  blocked, an immediate back aerial whiffs on the wrong side, and the
  forward-facing aerial control blocks while staying in front.
- Browser readiness exposes `mindgame_probe=1` only when the approach, all
  close/safe/far spacing outcomes, shield control, cross-up, early whiff, and
  front-block control pass together. The constituent native ground and aerial
  save/load oracles supply deterministic continuation evidence.
- Registry row 32, Mindgame, advances from `planned` to `playable`; broader
  conditioning history, the future grab mixup, and owner execution remain
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

## Explicitly preserved playtest requirements

- Keyboard clients must emit reduced horizontal magnitude for slow walk and
  full magnitude for dash/dash-dance. They must also emit reduced vertical
  magnitude so shield platform drop remains distinct from full-down spot dodge.
- Jump release during jump squat selects one short-hop speed; holding through
  jump squat selects one full-hop speed. Hold duration after launch does not
  change either height.
- Airborne horizontal input changes drift velocity but never changes facing;
  an opposite-direction air jump likewise preserves the takeoff-facing
  direction.
- A fresh airborne trigger produces a neutral or directional air dodge.
  Diagonal surface contact preserves horizontal momentum through exactly ten
  special-landing ticks, enabling wavedash and waveland with keyboard input.
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
- Full direction plus light attack remains the standing forward smash; delaying
  light by one to three same-direction initial-dash ticks remains the
  range-extending small-step route. Frame 4 or releasing the direction must
  remain the ordinary non-smash ground attack.
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
- The back-aerial cross-up must begin in front while facing away, pass the held
  shield through air drift, and finish behind with facing preserved. The early
  attempt must whiff and the forward-facing control must remain in front.
- The mindgame route must reuse the same readable approach cue and branch only
  through legal player/responder input. The jab and held-shield responses must
  reward different continuations, while both wrong reads remain visible.
- The juggling route must record at least two legal hits with no grounded
  target state between them. Directional influence plus a fresh air dodge must
  make the attempted airborne follow-up visibly active but non-connecting.

## New binding M4.4 scope

- The governing plan now pins and enumerates all 61 unique techniques marked
  available for SSBM in the referenced advanced-technique table.
- This incremental slice does not claim full technique parity. Dash-dancing is
  verified; approach, auto-canceling, cross-up, dash canceling, dashing shield, drop cancel, edge dashing, edge
  hopping, fox-trotting, instant double jump, L-cancelling, pivoting, SHFFL,
  juggling, mindgame, shield platform dropping, short hop air dodge, small step forward smash,
  sharking, spacing, tech-chasing, V-cancelling, and wavedash are
  now playable; other rows
  remain lower evidence states until their full
  movement, combat, item, team, or fighter-content dependencies are present.
- A versioned row-by-row registry, deterministic evidence links, and browser
  playtest recipes are required for all 61 rows before M4 can be accepted; none
  may be deferred to a later milestone.
- Registry schema 1 now exists at
  [`m4_advanced_technique_registry.md`](../product/m4_advanced_technique_registry.md)
  and is mechanically checked for all 61 ordered rows. Its current gate is
  blocked: 1 verified, 29 playable, 6 primitive-ready, and 25 planned.
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

- Remaining ground attacks, aerials, specials, recovery, grabs/throws, analog
  light shield, shield size/tilt/pokes and shield SDI,
  projectile powershield/reflection,
  expansion of the powershield-cancel router to each future ground action,
  complete knockback/angle data, stale-move behavior,
  prone-orientation-specific getup-roll timing, a moving revival platform,
  and journal producers for every remaining action.
- Local setup/menu flow, replay-file event visualization,
  collision/hitbox overlay, and repeated verifier/human matches.
- Representative M4 performance/profile evidence and the mandatory owner
  combat playtest.

## First-slice verification

- Release workflow: 18/18 tests.
- Address/undefined-behavior sanitizer workflow: 18/18 tests; leak discovery
  disabled only for the restricted workspace.
- Mechanical oracles: 170 movement invariants, 312
  attack/reaction/shield/floor/surface
  invariants plus 30 combat-journal invariants, 24 stock/respawn/result
  invariants plus 44 match-journal invariants,
  and separate 20,000-tick deterministic four-player traces.
- M2 kernel compatibility: movement, snapshot, RL, replay, and forbidden-symbol
  checks passed after the state-schema migration.
- Native replay corpus: exact 180-tick
  attack/reaction/shield/ground-dodge/air-dodge trace at 31,303
  bytes,
  replay SHA-256
  `36452288611860eea89e051f26ce32dfe1a431537a0dae9bb7379047eadf1c2f`,
  final SHA-256
  `d015347ede291c4f8f3dd08cc794ac12d04a74bc1b789d5ecb86facef7e36745`,
  and event-journal SHA-256
  `d2f5992ecc10cd4fb54a6c7bb5165e2983b019207b76c3792cc4bde4379be14f`;
  local native/WebAssembly output is byte-identical and CI repeats it.
- Clean Chrome CI remains the generated-Wasm, canonical replay-inspector, and
  live-playtest DOM gate.

## Browser-adapter verification

- Strict-warning native adapter contract: pass
  (`walk_axis=13500`, `dash_axis=32767`,
  movement/fox-trot/pivot-dash-cancel/dashing-shield/small-step-forward-smash/drop-cancel/V-cancel/approach/spacing/sharking/cross-up/mindgame/juggling/
  edge-hop-and-dash/
  ground-dodge-and-roll/air-facing/
  air-dodge-and-wavedash/
  aerial-auto-cancel-and-L-cancel/strong-aerial-30-vs-15-landing/
  combat-and-event-journal/reaction/shield-PSC-and-shield-break/default-tumble/
  floor-recovery/tech-chase/surface-tech
  /stock-respawn probes and live rendering).
- Browser-standard gamepad mapping/polling contract: pass for synthetic mapping,
  axis quantization/dead zone, D-pad override, button routes, non-standard
  rejection, two-slot assignment, and live `navigator.getGamepads()` polling;
  representative hardware remains an owner check.
- Address/undefined-behavior sanitizer adapter contract: pass.
- Emscripten 6.0.3 build and native/WebAssembly replay comparison: pass.
- Browser JavaScript syntax and M1 source-boundary checks: pass.
- Focused owner controls and expected results:
  [`M4_browser_playtest.md`](M4_browser_playtest.md).
- Generated-page execution: pass in the clean Chrome CI lane.

M5 content scaling remains blocked until M4 combat feel is approved.
