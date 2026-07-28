# M4 SSBM advanced-technique registry

- **Registry schema:** 1
- **Pinned source:** [SmashWiki advanced-technique table, revision 2048934](https://www.ssbwiki.com/index.php?title=Advanced_technique&oldid=2048934#List_of_advanced_techniques)
- **Captured:** 2026-07-28
- **Scope:** 61 unique non-character-specific rows marked available for SSBM
- **M4 acceptance:** Blocked until every row is `verified`

The source table lists dash-dancing twice. This registry records it once.
Character-specific techniques are excluded here and are a separate M8 gate.
Every target is M4.4; no row may be deferred to a later milestone.

Statuses are monotonic evidence states:

- `planned`: one or more production mechanics or fixtures are absent.
- `primitive-ready`: relevant canonical mechanics exist, but the complete
  technique is not yet playable.
- `playable`: the technique can be performed through ordinary match/browser
  input, but some required positive/negative, cross-target, replay, or owner
  evidence is incomplete.
- `verified`: the complete behavior has deterministic positive and negative
  oracles, a verifier-readable trace, save/load/replay/rollback and
  native/WebAssembly evidence, and a passing browser recipe.

Planned recipes describe the eventual human gate; they are not claims that the
current build can perform the technique.

| # | Technique | Status | Target | Dependencies and supporting configuration | Automated evidence | Browser playtest recipe |
|---:|---|---|---|---|---|---|
| 1 | Approach | planned | M4.4 | Complete neutral interactions, attacks, grab, projectile, and legal match loop | — | Future: close distance with walk, dash, platform, and aerial routes against a responding opponent and convert without scripted state |
| 2 | Auto-canceling | planned | M4.4 | Aerial attacks, landing windows, and landing-lag data | — | Future: land the same aerial inside and outside its auto-cancel window and compare action/lag |
| 3 | Bat dropping | planned | M4.4 | Original bat-like item, pickup, aerial drop, item hitbox, and reset | — | Future: carry the test item airborne, drop it onto a legal target, and observe its production hit |
| 4 | Boost grab | planned | M4.4 | Dash attack, grab, and the legal cancel/momentum-transfer window | — | Future: compare ordinary dash grab range with the timed dash-attack-to-grab sequence |
| 5 | Camping | planned | M4.4 | Projectile/resource loop, stage positions, opponent policy, stock clock, and anti-stall trace | — | Future: maintain repeatable safe distance with legal resources while the opponent actively approaches |
| 6 | Chain grab | planned | M4.4 | Grab/throw, escape, regrab windows, percent-dependent victim states | — | Future: perform two or more legal regrabs and show the negative timing/percent case escapes |
| 7 | Charge storage canceling | planned | M4.4 | Chargeable action, stored charge state, cancel path, and resumed use | — | Future: charge, cancel without losing the stored value, then execute the charged action |
| 8 | Cross-up | planned | M4.4 | Aerial/dash attacks, pass-through positioning, facing, block, and punish states | — | Future: land the same attack in front of and behind shield and inspect final side/facing |
| 9 | Dash cancel | planned | M4.4 | Melee-compatible dash-cancel state/input routes and follow-up actions | — | Future: cancel initial dash through each legal route and prove the same input outside its window fails |
| 10 | Dash-dancing | verified | M4.4 | Production initial-dash reversal, normalized full-axis keyboard input, replay/state schema | `tests/sim/test_m4_movement.c`; `src/web_client/m4_playtest.c`; `tools/verify_m4_movement.sh`; `tools/verify_m4_browser.sh` | Tap full left/right during the ten-tick initial dash and confirm every reversal remains `INITIAL DASH`; repeat after `RUN` and confirm it becomes `RUN TURNAROUND` instead |
| 11 | Dashing shield | planned | M4.4 | Distinct tap/release shield momentum behavior and timing oracle | — | Future: tap then release shield from the legal dash state and compare travel with held shield stop |
| 12 | Double jump cancel | planned | M4.4 | Cancelable double-jump fixture, aerial actions, vertical-velocity cancellation | — | Future: double jump, attack in the cancel window, and compare the resulting short aerial arc with a late attack |
| 13 | Double jump cancel counter | planned | M4.4 | Double jump cancel plus defensive counter action and hit resolution | — | Future: counter an incoming attack during the double-jump-cancel route and compare an early/late miss |
| 14 | Drop cancel | planned | M4.4 | Platform drop, aerial attack, landing/cancel windows | — | Future: drop through the platform, attack, and land inside the cancel window; repeat one tick late |
| 15 | Edge dashing | planned | M4.4 | Ledge invulnerability, ledge jump/release, air dodge, wavedash landing | — | Future: leave the ledge, air dodge onto stage, and measure actionable/invulnerable overlap |
| 16 | Edge hopping | planned | M4.4 | Complete ledge release/jump options, aerial actions, invulnerability | — | Future: release and jump from ledge into a legal aerial; compare with neutral hang |
| 17 | Fox-trotting | planned | M4.4 | Consecutive initial-dash restart windows and character data fixture | — | Future: rhythmically re-enter initial dash without reaching run and compare with held run |
| 18 | Glide toss | planned | M4.4 | Item carry/drop, roll, momentum transfer, throw/drop input priority | — | Future: initiate roll and item throw/drop in the legal window, then compare item and fighter travel |
| 19 | Gimp | planned | M4.4 | Recovery actions, edgeguard attacks, stocks/KO, repeatable victim policy | — | Future: force a low-percent recovery failure through legal edgeguarding and prove the unchallenged recovery succeeds |
| 20 | Infinite | planned | M4.4 | Multi-action combo fixtures, escape policy, repeated-state detector | — | Future: demonstrate a repeatable non-escaping loop and an input/state change that breaks it |
| 21 | Instant double jump | planned | M4.4 | Airborne jump timing, jump-squat exit, exact first-airborne-frame oracle | — | Future: double jump on the first legal airborne frame and compare height/timing with a delayed input |
| 22 | Jab cancel | planned | M4.4 | Jab sequence, cancel windows, follow-up actions, hit/whiff variants | — | Future: cancel jab on hit and whiff inside the legal window and repeat one tick late |
| 23 | Jab reset | primitive-ready | M4.4 | Existing knockdown foundation plus grounded jab reaction/reset and get-up choices | `tests/sim/test_m4_combat.c` covers knockdown only | Future: jab a knocked-down target into the reset state and compare invulnerable/late cases |
| 24 | Juggling | planned | M4.4 | Aerial attacks, repeated launch, recovery/DI policy, stock result | — | Future: keep a target airborne through multiple legal hits while a DI/escape policy remains active |
| 25 | Jump cancel throw | planned | M4.4 | Item carry/throw and jump-cancel input priority | — | Future: enter jump then throw the held item inside the cancel window; compare one tick late |
| 26 | Jump-canceled grab | planned | M4.4 | Grab action and jump-cancel routes from shield/run states | — | Future: input jump then grab inside the legal cancel window and inspect standing-grab behavior |
| 27 | Jump-cancelling | planned | M4.4 | General jump-cancel router for supported attacks/specials/grab | — | Future: cancel jump startup into every supported legal option and prove disallowed options remain jump |
| 28 | Kill confirm | planned | M4.4 | Stocks/KO, multiple attacks, percent/DI-dependent true-sequence oracle | — | Future: execute a setup-to-KO sequence at its valid percent and show the negative percent/DI case escapes |
| 29 | L-cancelling | planned | M4.4 | Aerial attacks, trigger timing before landing, normal/reduced landing lag | — | Future: press trigger inside and outside the L-cancel window and compare landing-lag ticks |
| 30 | Ladder | planned | M4.4 | Repeated upward aerial attacks, platforms, DI/escape policy, KO | — | Future: carry a target upward through repeated legal aerial hits and show an escape input breaks the route |
| 31 | Ledge-cancelling | planned | M4.4 | Action-specific landing/ledge transitions and cancelable recovery fixture | — | Future: contact a ledge during the supported action to cancel lag; repeat at non-cancel geometry |
| 32 | Mindgame | planned | M4.4 | At least two credible movement/defense/attack branches and responding opponent policy | — | Future: use the same approach startup to produce two legal outcomes that punish opposite responses |
| 33 | Moonwalk | planned | M4.4 | Melee stick-history sampling, turnaround momentum, traction, and facing oracle | — | Future: perform the stick sweep and confirm backward slide while facing forward; compare a mistimed sweep |
| 34 | Powershield | playable | M4.4 | Four-tick physical window, dense shield, attack block; projectile two-tick reflection remains | `tests/sim/test_m4_combat.c`; `src/web_client/m4_playtest.c`; `tools/verify_m4_combat.sh`; `tools/verify_m4_browser.sh` | Raise shield immediately before the physical attack connects; confirm powershield indicator, zero attack shield damage, ordinary stun, and larger pushback; raise it five or more ticks early for the negative case |
| 35 | Pivoting | planned | M4.4 | Turnaround frame, action interrupts, facing and velocity preservation | — | Future: reverse from run and act on the legal pivot frame; repeat early/late |
| 36 | Planking | planned | M4.4 | Ledge invulnerability/regrab limits, aerial options, stocks, anti-stall policy | — | Future: repeat a legal ledge refresh sequence while an opponent threatens the ledge and inspect vulnerability gaps |
| 37 | Power shield canceling | planned | M4.4 | Powershield result, release timing, one-frame physical attack cancel, full ground-action set | Powershield result state exists; cancel path absent | Future: powershield a physical hit, release, and attack on frame 2 of shield drop; compare normal shield's 15-tick release |
| 38 | Scar Jump | planned | M4.4 | Ledge jump/release, wall/stage geometry, recovery and aerial routes | — | Future: execute the named ledge route on original equivalent geometry and compare a missed wall/ledge timing |
| 39 | Sharking | planned | M4.4 | Platform geometry plus aerial/upward attacks and target shield/movement | — | Future: threaten and hit a platform opponent from below while remaining in legal stage space |
| 40 | Shield break combo | primitive-ready | M4.4 | Shield HP/break state exists; complete launch, knockdown, vulnerable mashable stun, and combo route remain | `tests/sim/test_m4_combat.c` covers depletion, placeholder re-hit lockout, and deterministic reset only | Future: force shield break, follow the complete break sequence, and land a deterministic punish before stun ends |
| 41 | Shield platform dropping | planned | M4.4 | Shield tilt/trigger routing, platform drop during shield, precise release/cancel rules | — | Future: shield on a pass-through platform, perform the drop input, and compare ordinary shield release |
| 42 | Shield-stop | playable | M4.4 | Run, dense shield, retained momentum, traction; initial dash exclusion | `tests/sim/test_m4_combat.c` operations `shield-stop-and-entry` and `initial-dash-cannot-shield` | Reach `RUN`, press shield, and confirm forward shield slide under traction; press it during `INITIAL DASH` for the negative case |
| 43 | Shine spike | planned | M4.4 | Original reflector-like action, aerial hit, recovery victim, stock/KO | — | Future: use the reflector-like aerial hit to deny recovery and prove the unchallenged target survives |
| 44 | Short hop laser | planned | M4.4 | Binary short hop exists; original projectile action, aerial firing, landing route remain | `tests/sim/test_m4_movement.c` covers short hop only | Future: short hop, fire the original projectile at legal heights, land, and compare full-hop timing |
| 45 | Short hop air dodge | planned | M4.4 | Binary short hop plus directional air dodge and landing behavior | `tests/sim/test_m4_movement.c` covers short hop only | Future: short hop then air dodge at the first legal frame; compare grounded shield and late air dodge |
| 46 | Short hop fast fall l-cancel | planned | M4.4 | Short hop/fast fall exist; aerial attack and L-cancel landing windows remain | `tests/sim/test_m4_movement.c` covers movement constituents only | Future: perform the full short-hop aerial, fast-fall, and L-cancel sequence and compare each omitted input |
| 47 | Small step forward smash | planned | M4.4 | Reduced walk exists; forward-smash action/input timing and facing remain | `tests/sim/test_m4_movement.c` covers reduced walk only | Future: take the minimum legal step then forward smash; compare neutral smash range |
| 48 | Smash directional influence | playable | M4.4 | Target hitlag, component-edge SDI, collision-safe displacement; shield SDI remains separate shield work | `tests/sim/test_m4_combat.c`; browser reaction startup probe; 180-tick replay corpus | During hitlag, cross into one stick component and observe one pulse; hold it to prove no repeat, then add the second component |
| 49 | Spacing | planned | M4.4 | Multiple hitbox ranges, shield/whiff outcomes, responding opponent policy | — | Future: place the same attack at safe tip and punishable close ranges and compare contact/punish state |
| 50 | Stage humping | planned | M4.4 | Exact pinned-source behavior research, edge/teeter/crouch state, input oracle | — | Future: execute the researched original-stage-equivalent sequence and compare the mistimed input |
| 51 | Stage spike | planned | M4.4 | Wall/ceiling collision, recovery, stage-impact launch, teching, stocks | — | Future: launch a recovering target into original stage geometry and compare successful/missed tech |
| 52 | Stalling | planned | M4.4 | Resource/ledge/air options, match clock or bounded trace, responding opponent | — | Future: repeat a legal time-extending route and show its resource or vulnerability limit |
| 53 | Taunt cancelling | planned | M4.4 | Taunt action, stage/ledge cancel interaction, recovery frames | — | Future: start taunt in the legal geometry, cancel it, and compare ordinary full taunt duration |
| 54 | Team wobble | planned | M4.4 | Narrow team laboratory, grabs/throws/pummel, ally hit interaction, escape oracle | — | Future: two allied fixtures perform the alternating grab sequence while a legal victim escape policy runs |
| 55 | Teching | primitive-ready | M4.4 | Ground tech in place/roll exists; wall/ceiling tech, invulnerability, and complete outcomes remain | `tests/sim/test_m4_combat.c`; browser reaction startup probe | Current partial recipe: trigger within 20 ticks of tumble landing for in-place/roll; future complete recipe adds wall and ceiling impacts |
| 56 | Tech-chasing | primitive-ready | M4.4 | Ground tech outcomes exist; get-up choices, attacks/grab, reaction policy, repeatable chase route remain | `tests/sim/test_m4_combat.c` covers outcome selection only | Future: cover at least two legal tech/get-up outcomes through reaction rather than scripted state |
| 57 | Teeter cancel | planned | M4.4 | Teeter action at support edge and legal cancel routes | — | Future: enter teeter at the edge, cancel with the legal input, and compare ordinary teeter duration |
| 58 | Turtling | primitive-ready | M4.4 | Dense shield exists; grab, rolls, spot dodge, shield tilt/poke, out-of-shield offense remain | Shield block tests cover only one defensive constituent | Future: sustain a legal defense sequence using shield and multiple escape/punish options against a responding attacker |
| 59 | V-cancelling | planned | M4.4 | Timed trigger before hit, aerial launch reduction, exact exclusions and oracle | — | Future: press trigger in and out of the legal pre-hit window and compare launch velocity |
| 60 | Wavedash | planned | M4.4 | Jump squat exists; directional air dodge, ground collision, slide/traction, angle rules remain | `tests/sim/test_m4_movement.c` covers jump squat only | Future: jump and air dodge diagonally into the floor, then compare angle-dependent slide and late air dodge |
| 61 | Zero-to-death combo | planned | M4.4 | Complete combat/stock loop, multi-action combo, DI/escape policy, verifier trace | — | Future: begin at zero percent and reach a legal KO without neutral reset while the target executes the declared defense policy |

## Current gate summary

The registry currently has 1 `verified`, 3 `playable`, 5
`primitive-ready`, and 52 `planned` rows. M4 acceptance is therefore blocked.
Advancing a row requires adding its exact evidence here in the same change;
adding a primitive without updating this registry is a plan-compliance failure.
