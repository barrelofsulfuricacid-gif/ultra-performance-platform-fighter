# M0 original stage briefs

## Shared stage rules

- Every stage has original geometry, naming, art, animation, music, and hazards.
- Gameplay geometry is built from simple deterministic collision primitives and
  fixed-capacity hazard state.
- Every gameplay hazard has a hazard-off competitive form.
- Presentation-only particles, foliage, crowds, water, and lighting never enter
  deterministic state.
- Every stage declares 1v1, 2v2, and capture-the-flag eligibility rather than
  silently producing broken mode layouts.
- Final geometry is tuned only after the M0 representation checkpoint.

## Ten-stage release set

### S01 — Winter: Glacier Observatory

- **Layout:** Broad central ice shelf, two elevated stone instruments, and a
  lower pass-through bridge.
- **Hazard:** A deterministic aurora pulse forecasts a lateral gust. Thin outer
  ice panels crack through three visible states before temporarily dropping.
- **Hazard-off form:** Fixed stone geometry replaces breakable ice; no gust.
- **CTF concept:** Bases occupy the two stone instruments, with the central
  shelf as the contested route.
- **Performance concern:** Ice state is per panel, not a deformable surface.

### S02 — Autumn: Emberleaf Canopy

- **Layout:** Central ancient trunk platform with asymmetric branch platforms
  that form high and low routes.
- **Hazard:** Seeded wind cycles bend two branches between known endpoints and
  release a clearly telegraphed wave of falling seed pods that apply light
  knockback.
- **Hazard-off form:** Branches remain stationary; seed pods are visual only.
- **CTF concept:** Opposing canopy nests with a risky fast route across the
  upper branches and a safer route around the trunk.
- **Performance concern:** Branch motion uses table-driven transforms.

### S03 — Summer: Sunwheel Fair

- **Layout:** Wide festival plaza with two awnings and a central rotating
  sunwheel platform above the ground.
- **Hazard:** Firework mortars telegraph lanes before upward launch pulses; the
  sunwheel advances through a short deterministic position cycle.
- **Hazard-off form:** Static central platform and background-only fireworks.
- **CTF concept:** Bases sit under the awnings; the wheel creates a contested
  aerial shortcut.
- **Performance concern:** Fireworks use lane masks rather than free particles
  for gameplay collision.

### S04 — Desert: Sirocco Vault

- **Layout:** Ruined stone arch over a shallow central sand basin with two
  narrow side platforms.
- **Hazard:** The basin fills and drains on a forecast cycle, changing traction
  and exposing a low tunnel; a marked sirocco pushes airborne fighters only.
- **Hazard-off form:** Basin remains firm and level; no wind.
- **CTF concept:** Bases are on opposite ruins; the tunnel is a temporary
  low-risk flag route.
- **Performance concern:** Sand is a region modifier, never a particle or fluid
  simulation.

### S05 — Tropical forest: Monsoon Crown

- **Layout:** Layered giant-root arena with two suspended vine platforms and a
  central canopy gap.
- **Hazard:** Rain fills two root channels, producing short directional flows;
  vines swing through precomputed arcs after a visible animal-call cue.
- **Hazard-off form:** Dry channels and stationary vine platforms.
- **CTF concept:** Bases occupy opposite root hollows; vines enable high-risk
  captures over the canopy gap.
- **Performance concern:** Water is a timed region force and vine motion is a
  lookup table.

### S06 — Rocky mountains: Thunderhead Pass

- **Layout:** High central ridge, two lower shelves, and narrow climbable
  approaches from each side.
- **Hazard:** Small rockfalls travel only through forecast lanes; a distant
  thunder cue precedes a short updraft along one shelf.
- **Hazard-off form:** No falling rocks or updraft.
- **CTF concept:** Bases are recessed into opposing cliffs; ridge control
  shortens the return route.
- **Performance concern:** Rockfalls are pooled deterministic entities with a
  strict cap.

### S07 — Sea: Leviathan Wake

- **Layout:** Deck of an original ocean research vessel with two mast platforms
  and low bow/stern extensions.
- **Hazard:** A visible swell tilts the deck among quantized angles; a wave
  crosses one marked deck level and applies momentum without altering geometry.
- **Hazard-off form:** Level deck and visual-only sea motion.
- **CTF concept:** Bases occupy bow and stern; mast routes avoid the wave but
  expose carriers to aerial attacks.
- **Performance concern:** Deck collision uses a small set of prevalidated
  transforms, not continuous rigid-body physics.

### S08 — Volcanic forge: Cinderworks

- **Layout:** Central anvil platform, two suspended smelting trays, and safe
  outer maintenance ledges.
- **Hazard:** A mechanical hammer strikes a telegraphed central zone; molten
  channels alternate between inactive and damaging states.
- **Hazard-off form:** Hammer locked overhead and channels covered.
- **CTF concept:** Bases sit on the maintenance ledges; the center is the fast
  but dangerous crossing.
- **Performance concern:** Hazard zones are bit-mask candidates in M0.

### S09 — Bioluminescent cavern: Lumen Deep

- **Layout:** Central crystal shelf, two mushroom-like pass-through platforms,
  and low side pockets.
- **Hazard:** Pressure vents emit upward bursts in a deterministic pattern;
  crystal pulses temporarily reveal and activate one of two short bridges.
- **Hazard-off form:** Both bridges remain solid and vents are inactive.
- **CTF concept:** Bases occupy side pockets; bridge state changes the fastest
  carrier lane.
- **Performance concern:** Bridge/vent state is represented by a few bits.

### S10 — Clockwork city: Meridian Engine

- **Layout:** Central clock face, two gear platforms, and narrow side balconies.
- **Hazard:** Gear platforms step among discrete positions; a sweeping hand is
  a slow, clearly visible moving collider that can be shielded or jumped.
- **Hazard-off form:** Gears lock into a symmetric competitive layout and the
  hand becomes background-only.
- **CTF concept:** Bases are on the balconies; gear alignment periodically
  opens a direct central route.
- **Performance concern:** All motion follows integer phase tables shared by
  rollback and replay.

## Stage-wave acceptance

For each stage:

- Hazard-on and hazard-off states save, restore, hash, replay, and rollback.
- Four-fighter worst-case play has no dynamic allocation or unbounded entity
  creation.
- Gameplay cues remain readable with full original presentation.
- The verifier has deterministic traces for every hazard transition.
- The owner playtests the stage before its content wave closes.
