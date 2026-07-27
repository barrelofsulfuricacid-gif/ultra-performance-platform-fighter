# M0 Melee-feel contract

## Purpose

This document defines the gameplay properties that the original game must
preserve. It is a behavioral target, not permission to copy protected
expression or extracted implementation data.

The governing fidelity choice is D1-A: every playable SSBM fighter/form gets an
original mechanical counterpart with substantially the same move functions and
matchup identity. Names, characters, worlds, visual/audio assets, lore,
animation, presentation, and implementation data remain original.

## Core feel

The game must feel:

- Immediate: local input affects simulation on the next eligible 60 Hz tick.
- Precise: analog direction, timing, state, and position explain outcomes.
- Fast: movement and recovery from actions support sustained interaction.
- Expressive: players can combine universal movement with fighter-specific
  mechanics rather than follow long predefined strings.
- Punishing but interactive: openings matter, while DI, SDI, teching,
  recovery mix-ups, and resource choices preserve defensive agency.
- Edge-centric: stage control, recovery, ledges, edgeguarding, and blast zones
  matter as much as raw damage.
- Discoverable but deep: basic actions are simple; timing, momentum, spacing,
  cancels, and matchups produce the depth.

## Fixed simulation contract

- Gameplay advances at exactly 60 logical ticks per second.
- Headless execution never sleeps and advances those same ticks as fast as
  possible.
- There is no universal action buffer. Any buffer, queue, leniency, or
  interrupt window is an explicit mechanic and design-data field.
- Inputs are normalized once per tick into deterministic digital buttons and
  quantized analog axes.
- Local, replay, rollback, verifier, and RL play call the same transition
  function.
- Presentation interpolation cannot alter collision, action timing, or state.

## System matrix

### Movement and stage interaction

| System | Required behavior |
|---|---|
| Analog ground movement | Walk speed varies with stick magnitude; dash, run, turnaround, and stopping have distinct states. |
| Initial dash | A brief, commitment-bearing initial dash supports dash-dancing, pivots, and character-specific dash lengths. |
| Dash-dance and foxtrot | Reversing during the initial-dash window and chaining dashes provide deliberate spacing tools. |
| Traction and momentum | Ground friction is fighter-specific and affects slides, landings, wavedashes, and punish options. |
| Crouch | Crouch changes profile and enables intentional dash/run cancellation and crouch-cancel interactions. |
| Jump squat | Jump has a fighter-specific grounded startup. Short/full hop selection is input-duration based. |
| Air movement | Gravity, fall speed, fast-fall speed, air acceleration, air-speed cap, and jumps are fighter-specific. |
| Fast fall | A deliberate downward input after the apex accelerates descent and interacts with aerial timing. |
| Pass-through platforms | Landing, dropping through, shield dropping if retained, platform movement, and edge cases are deterministic. |
| Air dodge | Directional air dodge has startup, invulnerability, momentum, and recovery data. |
| Wavedash/waveland | Air dodging into a surface intentionally converts air-dodge momentum into a grounded slide. It is a supported mechanic, not an accidental undefined behavior. |
| Ledges | Grab, occupancy, invulnerability, release, jump, roll, attack, get-up, refresh, hogging, and ledgedash interactions are explicit. |
| Blast zones | Stocks are lost by crossing stage-specific blast boundaries, not by a conventional health depletion rule. |

### Offense

| System | Required behavior |
|---|---|
| Normal move vocabulary | Jab, tilts, dash attack, charged smashes, five aerial directions, grabs/throws, and four special directions are supported where a fighter uses them. |
| Startup/active/recovery | Every action has explicit phases, interrupts, auto-cancel windows, landing behavior, and hitbox schedules. |
| Hitboxes and hurtboxes | Shapes, attachment, priority/clank behavior, damage, angle, base knockback, growth, effects, and target rules are data-driven. |
| Damage percent | Damage increases later knockback and creates matchup-dependent combo/KO windows. |
| Hitlag | Attacker and victim pause for impact readability and SDI opportunity while the simulation remains deterministic. |
| Hitstun/tumble | Knockback determines hitstun and tumble; actionable and non-actionable flight states are explicit. |
| Knockback | Damage, victim percent, weight, move parameters, launch angle, and modifiers feed an independently authored deterministic formula with Melee-equivalent function. |
| DI/SDI/ASDI | Launch-angle influence during knockback and positional influence during/after hitlag provide defensive agency. |
| Staling | Repeated move use may reduce damage/knockback through a data-driven recent-move queue if M4 tests confirm it is needed for matchup identity. |
| Grabs and throws | Grab beats shield, has whiff recovery, and enables fighter-specific throw follow-ups and team interactions. |
| Projectiles/items | Fighter-created projectiles and held objects are deterministic entities. A general party-item mode is not required by the source plan. |
| Edgeguarding | Off-stage attacks, resource denial, ledge occupancy, meteor/spike behavior, and recovery mix-ups are first-class. |

### Defense

| System | Required behavior |
|---|---|
| Shield | Shield health/size, analog strength where retained, stun, pushback, regeneration, poke exposure, release, and break are explicit. |
| Perfect shield | A narrow timing window changes block outcome and supports high-skill defense. |
| Out-of-shield actions | Jump, grab, roll, spot dodge, and fighter-specific options obey explicit state/cancel rules. |
| Crouch cancel | Grounded crouch can reduce eligible knockback/hitstun at low percent, with intentional counterplay and exclusions. |
| Rolls and spot dodge | Invulnerability and movement are finite, fighter-specific, and punishable. |
| Teching | Ground, wall, and ceiling impacts support in-place, directional, and missed-tech outcomes with lockout rules. |
| Meteor cancel | Eligible downward launch can be escaped after a defined delay using jump or recovery resources if retained by fighter identity tests. |
| Armor/intangibility | Double-jump armor, move armor, invulnerability, and intangible hurtboxes are separate explicit concepts. |

### Match flow

| System | Required behavior |
|---|---|
| Stocks | Configurable stock count, respawn, invulnerability, spawn selection, and simultaneous-KO handling. |
| Timer | Configurable match timer and deterministic timeout/tie handling. |
| 1v1 | Two independent fighters with standard stock rules. |
| 2v2 | Four fighters with team identity, configurable friendly fire, shared result logic, and readable ally effects. |
| Capture the flag | Combat remains active while flags, bases, carry/drop/return/capture, score, and overtime add objectives. |
| Hazards | Gameplay hazards are deterministic, rewindable, and separable from presentation-only effects. |

## Advanced-technique policy

The following are designed and tested as supported mechanics because they
materially shape Melee-like feel:

- Dash-dance, foxtrot, pivot, run/crouch cancel, and platform movement.
- Short-hop fast-fall aerial pressure.
- L-cancel-like landing-lag reduction with a precise pre-landing input window.
- Wavedash, waveland, ledgedash, edge cancel, and momentum transfer.
- DI, SDI, ASDI, crouch cancel, teching, and meteor cancel.
- Jump-cancel and action-cancel interactions that define fighter matchups.
- Fighter-specific movement such as float, double-jump cancel, multiple jumps,
  tether recovery, teleport angles, and duo desynchronization.

Unbounded infinites, freezes, crashes, stale-state corruption, or hardware
artifacts are not automatically preserved. A bounded technique must add skill,
counterplay, and matchup identity to enter the release contract.

## Human playtest rubric

Score each dimension from 1 to 5 after blind or randomized comparison of the
leading representation prototypes.

| Dimension | 1 | 3 | 5 | M0 pass condition |
|---|---|---|---|---|
| Input immediacy | Noticeably delayed or inconsistent | Generally responsive with occasional ambiguity | Every eligible input feels immediate and explainable | No candidate below 4 |
| Ground control | Sticky, slippery without intent, or hard to reverse | Usable spacing with some awkward transitions | Dash, pivot, crouch, and traction form a precise movement language | Leading candidate at least 4 |
| Air control | Little agency or excessive steering | Recovery and aerial spacing are usable | Drift, fast fall, jumps, and air dodge enable deliberate mix-ups | Leading candidate at least 4 |
| Collision stability | Snags, tunneling, jitter, or unexplained contacts | Rare visible anomaly | Contacts and platform transitions are stable and predictable | No critical anomaly |
| Combat clarity | Hits/launches feel arbitrary | Most outcomes are understandable | Spacing, timing, DI, and move properties explain outcomes | Leading candidate at least 4 |
| Combo expression | Predetermined or constantly interrupted | Some routes and defensive choices | Routes vary with percent, position, DI, resources, and adaptation | Leading candidate at least 4 |
| Recovery/edge play | Repetitive or hopeless | Several usable choices | Off-stage play creates layered attacker/defender decisions | Leading candidate at least 4 |
| Overall fun | Would not voluntarily replay | Promising but rough | Immediately wants another match | Leading candidate at least 4 |

The fastest representation is rejected if it creates a persistent gameplay score
below the pass condition.

## M0 exclusions

M0 does not lock:

- Final formulas or frame data.
- Fighter names, appearance, lore, or finished moves.
- Final stage geometry.
- Balance/tier outcomes.
- Presentation timing beyond the separation from simulation.

Those values are independently authored and tuned after the representation and
architecture checkpoint.

## Research basis

- [Super Smash Bros. Melee overview and mechanical differences](https://www.ssbwiki.com/Super_Smash_Bros._Melee)
- [SSBM technique index](https://www.ssbwiki.com/Category%3ATechniques_%28SSBM%29)
- [Wavedash behavior](https://www.ssbwiki.com/Wavedash)
- [The Melee Library](https://www.meleelibrary.com/)
- [Intermediate techniques index](https://strategywiki.org/wiki/Super_Smash_Bros._Melee/Intermediate_Techniques)
- [Advanced techniques index](https://strategywiki.org/wiki/Super_Smash_Bros._Melee/Advanced_Techniques)
