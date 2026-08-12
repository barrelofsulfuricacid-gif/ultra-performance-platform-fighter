# M4 SSBM behavior-fidelity audit

## Meaning of status

- `equivalent`: the relevant decomp route and numeric data have been mapped and
  deterministic verification exists for the supported scope.
- `partial`: the major route exists, but one or more frame, animation-command,
  collision, or character-data details still differ.
- `divergent`: the current behavior intentionally or accidentally differs.
- `missing`: no production-path counterpart exists.

No row implies whole-game equivalence. This audit covers the single M4 Falcon-
movement placeholder on the original laboratory stage.

Equivalence is behavioral. Small, bounded numeric differences caused by the
simulator's Q16.16 representation are acceptable when the verifier reports and
justifies them; discrete action/state, timing, facing, grounded-state,
threshold, and route differences are not.

### 2026-08-10 implementation checkpoint

An exhaustive callback-by-callback comparison against pinned decomp revision
`9509dc04406fb2028bfab01243841ba4787c0fb7` closed every discrepancy found in
the currently represented common/Falcon paths. The slice covers common input
priority and aging, movement, guard/shield break, damage response, grabs and
throws, rebirth, teeter, rebound/clank, and all Falcon specials, plus Falcon's
previously missing jab/angled-normal branches. Strict Windows and WSL release
suites, WSL ASan/UBSan, deterministic replay, and all registered stored domains
now pass. The newly imported ordinary-action geometry is additionally backed
by two semantically identical live Dolphin captures. Existing `equivalent`
labels still apply only to their stated route coverage; this validation does
not promote unregistered behavior to whole-character equivalence.

The consolidated 2,974-row executable-geometry capture now covers Jab 3,
rapid jab, every forward-tilt angle, and Falcon's real high/mid/low forward-
smash variants, with complete 11-capsule poses for all 26 concrete ordinary
action slots before pummel. A separate natural 3,142-row paired route imports
Pummel frame 0-23, its frame-4 attack sphere, CaptureWaitHi frame 0-34,
CaptureDamageHi frame 0-19, synchronized four-row hitlag, and the measured
53,806-Q16 capture anchor. Its independent repeat is semantically identical;
the generic stored domain protects all 79 poses and two active-frame geometry
controls. Remaining common-state poses stay explicitly incomplete.

## Current audit

| System | Status | Evidence and remaining gap |
|---|---|---|
| Base standing/Wait animation and collision pose | equivalent for the isolated complete uninterrupted idle lifecycle | A bounded native SquatRv entry and independent surface-memory pair qualify base Wait plus its exact six-update moving-target entry. Two byte-identical 440-row lifecycle captures prove the DAT-owned 70/20/10 Wait/Wait2/Wait3 state transitions, 60/75/70 clocks, zero-blend secondary entry, and six-update return/restart blends. Robust live endpoint seeds isolate unrelated global HSD consumers; an independent decomp/production theorem proves the exact HSD LCG and same-secondary rejection sequence. All 145 direct secondary poses / 1,595 capsules plus ECB agree within one Q16 unit; 32 stored lifecycle poses and a focused 440-tick native oracle protect production. |
| Stick aging, dead zones, dash/jump recognition | equivalent | Fresh horizontal/vertical tilt age, reversal reset, 0.80 dash threshold with two-tick dash window, and 0.6625 tap-jump threshold with four-tick window follow the common input/decomp routes. Just-below/above and slow/two-sample tap-jump controls match the executable. |
| Initial dash and dash physics | equivalent | One-shot Falcon 2.0 impulse with no entry-frame displacement, full A/B dash acceleration from the next frame, held transition after 15 displayed dash frames, and released completion after 28 displayed dash frames match the executable oracle. Every displayed Dash hurt pose is imported; a pinned Falcon Jab 1 route hits at 31.0 Melee units and misses at 31.5, while the old generic body rectangle misses the positive route by 3.503404617 units. |
| Walk/run acceleration and friction | equivalent | The generated typed view of Falcon's raw NTSC 1.02 common attributes drives the runtime friction-aware target/overshoot formulas; slow stick motion enters walk rather than dash. |
| Dash dance and backward dash acceleration | equivalent | A fresh reversal enters one displayed frame of smash `TURNING` with the old facing and damped velocity; a held reversal then enters opposite dash with the measured residual momentum plus Falcon's impulse. |
| Run braking and common IASA | equivalent for captured routes | A four-case/127-row exact acquisition theorem proves that straight full down from ordinary Run enters `RUN_BRAKE`, radial-gate diagonal down remains `RUNNING` for the edge row, down from RunBrake enters frame-1 `Squat`, and the locked Run phase following TurnRun rejects direct down. Source and production action/tick/facing/grounded payloads are identical at SHA-256 `dfa7be0339110c98c9107a069ef7e9751b14f2c174bd04a7e977c90ae745f6ad`. Neutral from terminal run produces 28 displayed `RUN_BRAKE` frames with Falcon's 0.08 friction before standing. Jump and main-stick down from RunBrake enter frame-1 `KneeBend`/`Squat`; shield-plus-down keeps crouch priority. Opposite stick on displayed brake frame 2 enters displayed TurnRun frame 1 with the old facing, resumed cursor, and 0.16 acceleration. Neutral guard, C-stick roll/spot, taunt, A, Z, and B remain in RunBrake. The executable and simulator agree for the captured IASA matrix. |
| Standing turn | equivalent for captured routes | Smash turn flips on the following frame and can enter dash; basic turn flips on displayed frame 8 and completes after displayed frame 11. All 11 displayed hurt poses are imported from matching accelerated/control captures. A fresh second-frame taunt applies the turn's facing flip first and then enters Falcon's 60-frame taunt. Timing and friction routes match the executable oracle. |
| Run turnaround | equivalent for captured routes | Full reversal from terminal run retains the old facing, applies full TurnRun acceleration, freezes displayed frame 9 until velocity crosses the common 0.01 threshold, flips facing on the following physics tick, resumes through displayed frame 21, and enters the ten-tick locked run route. All 22 source poses, including frame zero, are imported and source submotion is retained. The live oracle exposes one frame-9 update where gameplay facing has flipped but display bones still use the previous facing. Production derives that collision-facing phase from the existing tick/facing/direction tuple without snapshot state; combat and inspection consume it. Stored phase cases cover old-facing frame 9, the pending-display duplicate, and resumed frame 10. |
| Jump squat and takeoff momentum | equivalent | Falcon startup 4, 0.75 retained momentum, 0.95 stick contribution, and 2.1 cap are mapped. X/Y and main-stick tap jump match from idle, Landing, shield, and air. |
| Short/full hop | equivalent | Falcon 1.9 and 3.1 vertical velocities are converted to stage units. |
| Double jump | equivalent | Horizontal velocity is replaced from neutral/stick input using Falcon's 0.9 multiplier; vertical velocity uses the 0.9 multiplier. The decomp's interrupt-to-physics order is preserved: JumpAerial entry is immediately followed by ordinary air control in the same update. A natural opposite-stick route catches the former 0.36-versus-0.396 first-frame discrepancy. |
| Gravity, terminal velocity, air drift | equivalent | Falcon A/B acceleration, drift target, friction, gravity, terminal, and absolute horizontal cap are mapped. |
| Fast fall | equivalent | Requires a fresh downward tilt within four ticks after descent begins; holding down before the apex does not trigger it. |
| Crouch/crawl | equivalent for captured routes | Full-down input produces Falcon's seven displayed `Squat` frames, held `SquatWait`, ten displayed `SquatRv` frames, then standing. Exact 0.6875 entry and 0.625 release boundaries preserve the decomp's hysteresis. Jump, fresh guard, fresh taunt, neutral A, and down-special from all three states match, as do held-crouch dash/turn and release-state walk. Neutral B is accepted only from `Squat`. Physical Z enters `Catch` from `Squat`, but its A component falls back to `Attack11` from `SquatWait`/`SquatRv`, where catch is absent. `Squat` and `SquatWait` are crouch-cancel eligible while `SquatRv` is not; crawl entry remains disabled because Falcon cannot crawl. A separate Battlefield route proves the one-frame-down negative control and held-down `Squat` frames 1-3 into `Pass`. Every CrouchStart/CrouchEnd hurt pose and all 158 CrouchWait poses are imported; the latter wraps through an independent action clock and is protected by physical stored hit/miss controls. Jab 1 hits CrouchStart frame 3 at 17.7 units and misses at 17.84, while the old rectangle falsely reports +0.596595764 overlap for the miss. |
| Ground and platform collision | partial | The isolated 348-frame Battlefield route matches neutral jump-through ascent, the final ordinary-airborne descending crossing before `Landing`, 0.63 Pass entry speed, and same-frame solid-floor landing. It now runs the production source-derived stage profile rather than an authored vertical mimic and compares selected support on every grounded source row: left-platform source line 2/support 3 and main-floor line 1/support 2 agree throughout on Windows and WSL. Production preserves incoming vertical self velocity on the Landing entry row and projects it to zero on the next grounded-physics update. JumpF imports all 35 animation-driven ECB bottoms from raw/semantic digests `28c4e902d8860f6d02ec779004c67c7ab94f87c7f3970699cfd9a44a8844cf1d` / `6db927d319942e07d90ba6dd30aad39ad40bb42ab3cc09d498ea2587bfe233bb`; exact poses use the ordinary sweep and land on source route frame 95. `Pass` remains raw source submotion 209 for its complete 30-frame imported clock, independently of the nine-frame collision-skip timer; all 30 captured ECB bottoms are generated from raw/semantic digests `0dc57f8ffb85549be76b3b5a0017690b0df16905456169eaceaa2e7975eedc0c` / `90060e614f359189c32b25d76b780b3fa92861dfdcfae0fd357dcc07ec10e6f8`. CrouchWait adds all 158 source-frame-indexed four-point ECB poses under semantic SHA `ba47ef2736a5677d1909262a20f32991b7c2515407fae26626d5869b95edd265`; production retains its one-frame canonical animation clock and uses the same O(1) pose for floor, wall, and ceiling queries. The entire route passes within the 640-Q16 position envelope. Its platform-only probe independently reproduces Battlefield's complete 23-line runtime collision catalog, effective camera/blast bounds, stage kind, and four initial-player points. Production routes all five ceiling and twelve wall lines through the imported profile, with exhaustive primitive query coverage and representative central-underside production contacts. A dedicated live Battlefield theorem now selects ceiling line 10 and right-wall line 15 with their exact source normals, uses Falcon's complete 24-frame DamageFly top/side ECB, and compares 24 post-response samples under source/production digests `8a0c463ffae10b1567815013c85c500bcb25869727874086c96d0e9c522a2f68` / `107ea657a7bad069ea8ee02cb98306dd116b78838c8e6899a4adf9ff6fcf0982`. A second two-case theorem consumes the complete nine-pose `BOUNCE_CEILING` and 51-pose `BOUNCE_WALL` ECB tracks and matches 111 updates through first top-platform re-contact on samples 57/54 under source/production digests `4e9a0ad3222bd0d6b6d7ab7def0177cf4b5c361bded3826abfe2e91f9210dd5a` / `222a5504d62bc5500e57a88a0adad108b931ea73d2b70cdf46faccde3f36d2db`. The paired 48-row / 96-lane domain matches grounded push from both directions and controller ports. The imported four-case Hyrule line-34 slope and line-36/37 endpoint domain additionally qualifies slope-tangent landing projection, DamageFly ECB, DownBound contact/support ordering, collision-driven Fall, the exact eight-frame Fall animation loop, ordinary ledge entry through seven `CliffCatch` frames and first `CliffWait`, and the adjacent accepted/rejected controller samples around common-data `x480`; its 290-row source and 220-sample production digests are `9df8c72fca21359281d7d89391a9c363e08e6cf5c06db8873868e10521f27b49` and `73f3dae4bf726aedd1e2ab37911818faa9b3fff4d1a19ed2a92a41148f142f5d`. Fractional Walk/Run ECB and hurt geometry now use the shared HSD evaluator through exact six-frame transition ordering; five direct production ticks match live entry poses within 4 Q15 rotation and 4 Q16 translation units. Remaining special-action acquisition branches and broader action-specific ECB evolution remain open. |
| Ledge jump velocities | equivalent | Falcon 1.0 horizontal and 3.3 vertical attributes are mapped. |
| Other ledge actions | partial | Ordinary airborne reach consumes imported Falcon snap attributes, source-root/ECB coordinates, Hyrule endpoint/ledge flags, current/previous positions, and the imported `PlCo.dat` `x480` down-input rejection threshold. Adjacent quantized controls accept observed source Y `-0.65` and reject `-0.6625000238`; accepted catch clears all residual knockback channels on entry. Production preserves all seven `CliffCatch` frames and the live-qualified absolute catch/wait roots. The 19-case Hyrule pack qualifies quick/slow climb, roll, attack, two-phase jump, main/C drop arbitration and priority, C-stick attack/roll edges, exact 640/480 `CliffWait` timers with tumbling timeout, and the source-observable post-release regrab cooldown `29 -> 2 -> 1 -> 0`; production derives that boundary from the imported 30-frame common-data constant in source callback order. Two byte-identical no-fast-forward captures import all 434 displayed quick/slow option poses (4,774 capsules) through retained source submotion into combat and inspection. The generic stored digest covers all 714 common-plus-ledge-plus-guard poses. A separate byte-identical 143-row live pair proves exact quick-climb frame 29 against Falcon Jab 1 frame 4: actual-capsule hit/miss margins are `+0.573037244` / `-0.098777672`, while the generic rectangle falsely misses the positive route at `-1.895534515`. Remaining special-action acquisition/action-specific ECB branches keep the row partial. |
| Wall jump / wall and ceiling tech velocities | equivalent for captured routes | Falcon passive-wall, wall-jump, and passive-ceiling attributes are mapped. A five-case, 719-row Hyrule capture qualifies wall tech, wall-tech jump, wall reflection, ceiling tech with held drift, and ceiling reflection. Production consumes imported collision thresholds, 0.8 reflection, five-frame wall freeze, 14-frame wall-tech invulnerability, 15-frame reflection invulnerability, three-frame re-collision lock, and 31/45/26 action durations. The response comparator matches actions, timers, tumble/grounded/invulnerability, 0.49/-0.13 wall release, 1.39/2.97 wall-jump release, 0.06 ceiling drift, and frame-11 1.99 control release within 0.0015 source units. Absolute position remains outside that Hyrule route because its production fixture is a separate geometry domain. The first Battlefield theorem closes the impact geometry gap for its two sloped bounce routes: it checks selected line/normal, exact ECB point intersection, gravity-before-reflection ordering, both velocity channels, and relative position across 24 samples within 16/640 Q16 velocity/position allowances. The second Battlefield theorem imports each later bounce pose, keeps the real response clock distinct from ceiling's frame-eight display clamp, and matches the complete descent through first floor re-contact. |
| Normal landing lag and shared IASA | equivalent for captured routes | Falcon's four-frame value is mapped. Identical-input taunt, jump, dash/turn, guard, walk, direct-crouch, late-down-lockout, and ordinary-turn routes open or remain locked on the executable's exact boundary. First-frame down enters `SquatWait` directly; down one displayed frame later remains `Landing`. The no-input executable route still plays ordinary Landing's complete 30-frame source motion; every pose is imported. The live-qualified frame-22 Jab 1 discriminator hits at 20.3 units and misses at 20.6 while the generic rectangle misses both, and both controls are registered in the fast stored-equivalence domain. Character attack/special/grab content remains outside this row. |
| Aerial landing lag | equivalent | Distinct neutral/forward/back/up/down landing states select Falcon's 15/19/18/15/24 table; L-cancel states halve the selected value. Five independent natural Battlefield routes additionally prove Nair/Fair/Dair landing-lag entry and Bair/Uair auto-cancel entry, exact elapsed landing ticks, support, surface normal, position, and subsequent grounded velocities. |
| Shield input, light shield, size, tilt, and volume | equivalent for captured routes | A 500-frame pressure sweep matches sub-threshold through digital-full input, health, release, and regeneration. Three 283-frame light/intermediate/dense physical-hit captures additionally match integer shield-hit conversion, pressure-dependent health/stun, post-hitlag ordering, defender pushback, and separately decaying attacker recoil. The 270- and 2,158-frame memory-probed geometry captures qualify guard-angle and magnitude smoothing, all eight linear direction-animation keys, Falcon's joint-derived center, health/pressure radius, facing reflection, and the anisotropically mapped volume. Two independent 40-row captures additionally import all eight manually blended GuardOn poses, the frozen Guard pose, and all 16 ordinary GuardOff hurt/ECB poses. Six independent 159-row light/mid/dense hit captures qualify GuardSetOff's callback-derived fractional clock, receiving-pose hitlag freeze, and 18 dynamic hurt/ECB updates; hurt is exact and ECB differs by at most one Q16 unit. Nine stored observations plus production-path pressure tests protect that route. All six 99-frame identical-input replays additionally match 36 live/native source-clock rows from an explicit 22-unit placement while exercising imported Jab 1 spheres. A separate 2,568-frame, 33-decision Jab 1 sweep matches the decomp sphere/shield predicate and three last-hit/first-miss boundaries. |
| Shield-break orientation | equivalent for captured Falcon route | The common decomp samples terminal ShieldBreakFly `HipN->mtx[1][1]`; Falcon's pinned DAT pose yields `-3921` Q16 and therefore selects DownD, followed by StandD. Production consumes the generated one-bit result instead of the former hardcoded DownU branch. Two independent natural digital-shield depletion captures reproduce the exact `ShieldBreakFly -> DownD -> StandD -> Teeter` sequence and action counts. Other fighters require their own source-pose evaluation. |
| Roll, spot dodge, air dodge buffering | equivalent for captured routes | A 329-frame identical-input route covers forward roll, spot dodge, backward roll, held-L/fresh-R upward air dodge and landing, then horizontal LandingFallSpecial from a fresh down-left air dodge. EscapeN/EscapeF/EscapeB/EscapeAir duration and invulnerability come from their generated catalog/script rows. Both roll directions use the one source TransN stream selected by the decomp callback, including residual entry momentum, facing, and end position; no parallel authored displacement remains. EscapeAir force/dead zone/entry-frame decay come from `PlCo.dat`, displayed frame 30's variable-0 write switches the unchanged action to ordinary aerial physics, and a 48-frame live ECB-bottom sequence selects the exact landing tick. LandingFallSpecial copies incoming horizontal velocity to ground velocity, applies the common high-speed friction multiplier only above Falcon's walk maximum, then ordinary ground friction below it; its submotion has no TransN, and no authored displacement remains. Every captured flat-stage position delta equals post-friction velocity. Action, tick, grounded state, facing, invulnerability, and velocity compare strictly; position retains only the 640-Q16 representation envelope. Per-trigger edge tracking preserves held-L/fresh-R shoulder behavior. A separate 4,198-row geometry oracle imports all 32 SpotDodge poses, both distinct 31-frame roll tracks, all 49 AirDodge poses, the eight-frame FallSpecial loop, LandingFallSpecial's displayed source frames `1,4,...,28`, and all 30 ordinary Landing poses. It confirms SpotDodge vulnerable frames 1-2/21-32, roll vulnerable frames 1-3/20-31, and AirDodge vulnerable frames 1-3/30-49. Jab 1 hits SpotDodge frame 24 at 21.0 and misses at 22.0; RollForward frame 22 at 12.98 and misses at 14.18; RollBackward frame 24 at 20.00 and misses at 20.75; elevated AirDodge frame 31 at 21.0 and misses at 21.8; FallSpecial frame 5 at 15.5 and misses at 16.2; and LandingFallSpecial source frame 7 at 18.5 and misses at 19.3. The generic rectangle gives the wrong result for each discriminator. Uncaptured starting heights/momenta, slopes, platform edges, and collision cases remain broader-route work. |
| Tech and neutral getup body state | equivalent for captured routes | The generated script decoder covers all 318 submotions and exposes raw state-2/state-0 command frames. A four-case, 804-row Final Destination domain qualifies flat-floor missed tech, neutral tech, and both directional techs: 26/26/40/40 displayed frames, frames 1-20 tech invulnerability, retained hitstun memory, imported forward/backward `TransN`, and the source 220-frame DownWait value. Wall/ceiling tech timing, invulnerability, release velocity, and ceiling control are separately Dolphin-qualified. Both prone orientations qualify DownBound ECB contact, DownWait timeout/input priority, neutral getup, directional main/C-stick rolls, and buffered getup attacks; the Hyrule route adds exact slope-tangent roll response and endpoint departure. The two-case DownBound route imports 52 complete four-point poses under semantic SHA `3c4a4ce4586b11617aa99a08bac8709ea6d7aa8a179b5494c6f3f7fe4785c7df`. A separate eight-case, 1,150-row route imports both 70-frame DownWait loops, both 30-frame neutral getups, both 49-frame getup attacks, and all four 35-frame roll motions (438 poses) under semantic SHA `f519d632a88bcb582cb68865dd9a58d27e862fe619fc05d76ff3252ad5204f19`. Production preserves the source distinction between a direct terminal-DownBound U roll using the D motion and a DownWaitU roll using the U motion, and routes every imported pose through the common constant-time collision resolver. Broader stage shapes remain outside these captured routes. |
| Damage, knockback, hitlag, hitstun, DI/SDI | partial | The physical shield-hit subset is decomp-mapped and qualified at three pressure bands. Jab 1, jab 2, dash attack, the three tilts, the three explicit smashes, and the five directional aerials select imported per-sphere damage, angle, KBG, set-weight, BKB, and hitlag through Melee's fixed-point response. A six-case, 138-row live Falcon route qualifies open-air self/knockback channel separation, source-order 0.051 magnitude decay, neutral/full/half and squared-projection DI, radial SDI threshold behavior, analog displacement, and C-stick ASDI priority. A separate 64-row late-DashAttack route qualifies 15 flat-ground damage samples: Sakurai-angle projection, distinct `xF0_ground_kb_vel`, Falcon's 0.08 friction, `DamageLw1` timing, and the animation-plus-hitstun release boundary. Production ports the decomp's pre-launch ground/air x knockback-level x collided-hurtbox-height selector plus the strict 70/110-degree `DamageFlyTop` cone and 100%-plus-HSD-RNG `DamageFlyRoll` replacement, covering all 17 common Falcon damage motions. Roll applies the source-coordinate total-velocity `XRotN` override to one shared imported pose used by hurt and ECB consumers. Two independent physical DamageN2 captures qualify 288 pose observations / 3,168 capsules within one Q16 unit, including 132 damage-owned ECBs and all 12 mixed hit-entry ECBs owned by the preceding Wait variant after its animation callback; the native DamageLw1 route checks frame-one hitlag freeze, resumed source-clock progression, imported hurt geometry, and save/load. A fresh accelerated response-only surface capture contains Roll and passes all 145 rows across five cases; the prone live oracle supplies a velocity-oriented Roll ECB sample bounded below 0.024 simulation units in fixed-point production. Declared state, timer, and velocity fields match within 0.001 Melee units. Flat-floor player push is now independently registered, but the damage route's absolute position remains excluded until its complete stage/attacker interaction is bound. The wall/ceiling domain additionally qualifies reflected action/hitstun persistence, bounce invulnerability, tech clearing/preservation differences, and release/control velocities. The flat-floor and prone domains qualify missed/neutral/directional tech response, retained landing channels, DownBound contact, and getup branches. Slopes and wider attack interactions remain incomplete. |
| Attacks, grabs, throws, stale moves | partial | The pinned importer retains Falcon's complete 50-slot timing/effect/throw schema, action flags, and per-frame TransN deltas. Production consumes imported timing/effects for all three jabs, rapid jab, dash attack, every tilt/smash/aerial, standing/dash grab, pummel, normal throws, all Falcon specials, and the exact stale table. One 2,974-row live capture provides transformed hit geometry and complete 11-capsule poses for all 26 concrete ordinary action slots before pummel, including all five forward tilts and the three real forward-smash angles; an independent repeat reproduces the same semantic payload. The same pair independently qualifies source-derived four-point collision ECBs for all 26 ordinary routes across 2,086 observations / 1,850 unique frames at maximum one-Q16 error. Production routes all three jabs, every rapid-jab phase, Dash Attack, all five forward tilts, Up/Down Tilt, the three real forward-smash angles, Up/Down Smash, standing/dash grab, and all five aerials through the shared allocation-free HSD evaluator; generated per-motion offsets and animation flags own displayed-frame and TransN-reference mapping, while the common airborne callback owns aerial bottom locking. A native primitive covers all 925 runtime poses without action-specific geometry branches. A natural 3,142-row paired route adds the complete 24-frame pummel pose, frame-4 sphere, 35-frame CaptureWaitHi loop, 20-frame CaptureDamageHi response, synchronized hitlag, wait return, and measured capture anchor; its independent repeat is semantically identical. The generic stored domain hashes all 79 holder/victim poses and exact overlap/miss controls. The two absent forward-smash slots remain absent. Existing captures cover special, common, airborne, and ledge poses plus physical hit/miss boundaries that reject the generic rectangle. Ground normals use decomp IASA policies and callback-specific friction/root motion. A 1,250-frame identical-input capture qualifies aerial IASA and ordinary jump/fall clocks. Standing/dash grab, grabbability, throw release/response, inactive hit gaps, late effects, hitlag-frozen poses, source X/Y/Z, integer Melee response, state-2/state-3 moving hit-capsule collision, captured-target no-launch damage, and ordinary bystander pummel collision are production-routed. Other common-state poses, aerial-item/tether branches, and unsampled dynamics remain incomplete. Original custom strong-aerial/special fixtures remain outside Falcon equivalence. |
| Special moves and recovery | partial | All 17 Falcon special subactions, all 97 common-attribute words, the complete 35-field special-attribute block, raw animation translation, command timelines, effects, six Raptor Boost search spheres, and complete-frame executable pose/hit geometry are imported and hash-pinned. Both importers regenerate byte-for-byte from their pinned inputs. Ground/air Falcon Punch uses source duration, command-variable frames, root motion, angle/velocity attributes, ground/air collision transitions, pose, and hit geometry; two at-will direct Dolphin traces cover 200 frames each. Its generic stored domain protects the complete 100-sample ground route, the complete 100-sample air action clock, and the 51-sample air physics tail beginning at source frame 50; it deliberately does not claim the live comparator's reanchored early-air physics prefix. Ground/air Raptor Boost uses source 0.6 selection and 0.2 turnaround thresholds, ground velocity multiplier, frames 15-34/18-34 searches, frame-30 air gravity start, root translation, 7-damage hit effects, 20/40-frame miss/hit landing lag, and ground/air hit states. Its at-will 657-frame suite covers ground hit/miss, aerial miss, a complete 145-frame aerial-hit-to-floor route, 51 ground-edge frames, and a 155-frame native grounded-Capsule item-search route. The item route uses Melee's ambient spawner and item object, isolates the opposing fighter by at least 100 units, and selects the hit state at the first live source command-variable gate. The source predicate excludes the custom Relay Rod and ordinary weapon kinds. The fighter-hit route qualifies search conversion, imported frame-3 damage, hitlag, the entire natural pre-landing recovery tail, exact floor transition with preserved incoming vertical velocity, 40 hit-landing-lag ticks, and return to standing. The edge route qualifies the command-variable gate, full crossing root step, source air-speed clamp, zero-gravity transition row, and common `FallSpecial` continuation. All four Raptor start/hit motions derive complete four-point ECBs from the shared DAT/HSD evaluator, including TransN subtraction, the four-update airborne bottom lock, and prior-pose ownership on hit-state frame zero. Five captures qualify 423 observations / 411 complete frames within one Q16 source-to-live unit; the former 45-value memory-probed aerial-hit-bottom table is removed. Both miss routes reuse the exact imported common `FallSpecial` pose cycle, and the aerial transition applies common gravity. Ground/air Falcon Dive uses the source start/catch/throw actions, grab spheres, 5% catch, 12% throw, hitlag, grounded throw relocation, root motion, and distinct eight-frame `FallSpecial` ECB bottom. Its at-will verifier covers 116 grounded catch, 92 aerial catch, 103 grounded miss, 165 aerial miss, and two 63-frame aerial ledge-approach directions; 42 aerial victim frames additionally qualify 15.92% post-throw damage, zero launch, ordinary gravity, and the source-visible 26-frame reaction. Both ledge routes verify exact frame-64 `EdgeCatch` and frame-71 `EdgeHang` from live collision memory; the facing-away route additionally proves outward facing before contact and the common inward-facing catch transition. The exact-position Dive geometry theorem separately replays its capture-authored placement and rejects a translated miss control; natural identical-input dynamics remain distinct. Falcon Kick uses all seven source states, decoded wall/traction/edge/air-physics commands, root motion, hit geometry, imported traction, and ground-hit slowdown/cap. Its at-will 399-frame suite passes ground, air, landing, ground-edge, ground-hit, and Hyrule wall-rebound routes with strict state/velocity/hitlag checks plus the 640-Q16 position envelope, including the source collision conversion's half crossing-tick displacement and no same-tick gravity. The same 399 rows are bound to six cases in the generic stored registry with exact source-input equality and sparse field masks. The hit route additionally qualifies 15 damage, eight ticks of hitlag, the imported 0.6 speed modifier, and the separate ground/self velocity channels through ground end. The wall route qualifies action 363, preserved entry self velocity, ECB-lock-equivalent transition ordering, and the full rebound trajectory. The original Prism Burst, Arc Reservoir, and Vector Ascent fixtures do not count as equivalents. |
| Items, projectiles, reflector, charge | divergent | These are original technique-support fixtures rather than SSBM content tables. |
| Stage geometry, blast zones, spawns | partial | The Relay Rod laboratory remains an original test stage. A source-derived immutable Hyrule catalog covers MapCollData lines 34-37 with world-space vertices, normals, adjacency, endpoints, and ledge flags under semantic digest `4a0dd57bb8d9532589d3ecd129213d3a0876538a2dc7f733eca6c1e73c04db9c`; only this response slice is routed. Battlefield now has a complete source-derived 23-line collision catalog plus effective camera/blast bounds, stage kind 36, and all four initial-player points under address-free semantic digest `29525b7e0db4de8bf1a228f47e4216869ca362aff9d558a0c9ae81340103aa50`; two independent live captures regenerate it byte-identically. Its content constructor routes current floor/platform primitives, exact blast bounds, and settled initial supports. The 348-frame floor/platform route binds selected supports, while the two-case sloped-surface theorem binds ceiling line 10 and right-wall line 15, their source-space normals, exact DamageFly ECB contact, and natural reflection response. Pre-match entry choreography and remaining edge/action-specific collision branches are still open; complete Hyrule and full Battlefield behavior remain incomplete. |
| Stocks, respawn, match result | partial | Deterministic four-stock flow exists. The static slice imports and routes the source 60-tick descent, 240-tick wait, 120-tick invulnerability, and RebirthWait input priority. Dynamic stage/player revival targets, companion coordination, tournament rules, and entry choreography remain outside the compact model. |
| Released Damage versus DamageFall air dodge | equivalent for the captured callback split | Pinned `ftCo_Damage_IASA` delegates released ordinary airborne Damage to the ordinary Fall IASA table and therefore permits EscapeAir; pinned `ftCo_DamageFall_IASA` omits EscapeAir for released DamageFly/DamageFall. A two-case/68-row physical theorem proves fresh L enters `AIRDODGE` from non-tumble Damage while the same edge leaves DamageFall `TUMBLING` for two airborne rows. Source semantic SHA-256 is `7ce52b784989e56f7539b79dd779eed94ab41e4bcd624b980c263af0b916084b`; the two-case/four-sample stored trace pins production SHA-256 `cec3d2b1d9b67ad906bf68b074c8975f6e53bf48cc714650c6498bef7aeba93e`. Other unrepresented damage callbacks remain open. |
| Replay, save/load, rollback state, RL API | project-specific | These are deterministic project infrastructure and have no claim of equivalence to SSBM internals. |

The ordinary-airborne hurt-pose inventory is now source-complete for the six
runtime submotions currently retained by movement: JumpF, JumpB, JumpAerialF,
JumpAerialB, Fall, and FallAerial. Two independent unlimited headless captures
produce the same 186-pose / 2,046-capsule semantic payload SHA-256
`71c9e643816604f9d2e90cfc226b907e7ce7cb48edc4fa2fea51d6797013ee7f`.
Production uses one generated table under binary digest
`13763a74d044b686b5ae065e7120ac984d823cb8e31f7bf15597c16268277a72`;
the former 12-frame Dive-only JumpF copy no longer exists.

Current Falcon Dive boundary: independent facing-toward and facing-away ledge
theorems remain green, and production follows the decomp's bidirectional
start-state ledge query. The capture-authored aerial catch placement is now a separate exact-position
geometry theorem with one hit and one nearby miss control. It no longer mixes
that source setup with a different natural-jump trajectory. The 92 holder and
42 victim rows remain source evidence for their declared fields; they are not
presented as a general identical-input aerial-catch trajectory theorem.

## Blocking work before a behavioral-equivalence claim

1. Add live route coverage for still-unregistered common damage, shield,
   grab/throw, input, rebound/clank, rebirth, and Falcon action branches from
   the 2026-08-10 callback audit; native regression alone is not SSBM truth.
2. Import exact pose geometry for the remaining common states; current fallback
   envelopes outside the now-complete Wait/Wait2/Wait3 lifecycle are not
   equivalence claims.
3. Capture common-state hurt poses beyond Initial Dash/RunBrake/CrouchStart/
   CrouchEnd/KneeBend/SpotDodge/RollForward/RollBackward/AirDodge/
   FallSpecial/LandingFallSpecial/Landing/JumpF/JumpB/JumpAerialF/
   JumpAerialB/Fall/FallAerial and qualify remaining aerial-IASA branches and unsampled dynamic
   routes without duplicating constants. Looping motions require exact phase/
   rate reconstruction rather than a guessed linear pose track.
4. Represent and qualify dynamic rebirth targets/companions, broader stage and
   item kinds, aerial item/tether callbacks, and tournament choreography.
5. Complete the remaining hands-on native controller gate and re-run browser
   validation when a later slice changes the web adapter or Wasm content.

## Executable-oracle evidence

`tools/capture_ssbm_movement.py` drives an owner-supplied GALE01 NTSC-U 1.02
image through Dolphin/Slippi and records the post-frame action, facing,
position, velocity, and observed controller sample. `pf_m4_movement_trace`
replays those observed samples through the native simulator, and
`tools/compare_ssbm_movement.py` stops at the first behavioral divergence.

The post-build edit loop additionally runs
`tools/verify_ssbm_stored_equivalence.py`. Its generic registry selects affected
domain manifests, rejects stale generated rows, runs each filtered production
oracle, and then requires the pinned deterministic replay corpus. The first
registered domain, `falcon-common-hurt`, hashes all 714 production-accessed
poses and runs 20 manifest-owned hit/miss controls. Its 255 original common
poses, 434 quick/slow ledge poses, and 25 GuardOn/Guard/GuardOff poses share one source-submotion-aware generic
runner under source/production SHA-256
`9688be9b0ca0d0eacac5ba26714968acdcc3b19aaac9449778c108275b1c940b` /
`0641ed13ea1d179e214f5629b4f8d7b93e226091b9925d6e31dfe53645c74c36`.
The complete seventeen-domain / 101-case gate plus replay passes in 1.458
seconds on Windows and 0.575 seconds in WSL. Raptor Boost contributes five cases / 502 samples through the
generic production-CSV runner with the live comparator's exact field masks and
per-row clock exclusions. The natural airborne-landing case adds 520 samples
covering JumpB, JumpAerialF/B, looping FallAerial, Landing, supports, surface
normals, and all movement channels. The aerial-attack landing domain adds five
cases / 685 samples covering Nair/Fair/Dair landing lag and Bair/Uair
auto-cancel landing while consuming the complete 195-pose aerial ECB profile.
This is regression against already-
qualified live truth; it does not turn uncovered routes into evidence or replace a fresh
Dolphin qualification when a golden changes. The corresponding live common-
hurt pack executes 16 checkpoint-isolated cases / 323 rows in one headless/null/
unlimited ExiAI process and serializes the declared common and guard poses plus
the dash discriminator rows. A separate byte-identical no-fast-forward capture
supplies all 434 ledge poses. The combined live verifier reproduces the
714-pose source digest. The
separate two-case ledge collision projection reproduces raw/semantic SHA-256
`f31de47e694e46bf2269945747c97238ce443ddf88cbadc0a8e4214026f2785d` /
`fbe0cf877402bf82aba10d8ae3dceecb4e431caa87d9b75b5844bfb7b132af2d`
and rejects the former generic rectangle at the positive spacing.

The second registered domain, `falcon-common-damage-response`, uses the same
registry and an allocation-free numeric-trace runner. Six manifest-owned cases
compare three canonical samples each, covering neutral, full and half DI,
radial and below-radial SDI, and C-stick-priority ASDI. Its source observation
SHA-256 is
`51402cd3605ba2761e3c11ed6baab74eb1b7ab22136822507b39d0a00cc40d95`
and its production trace SHA-256 is
`91f1664c3c81795cf10bcfd6777a6d5934f48017cb073dabe9fb4d98fe9b745e`.
The live damage pack completes its 138 rows in 0.665 seconds warm and 3.438
seconds for the full process lifecycle.

The third registered domain, `falcon-common-ground-knockback`, contributes one
late-DashAttack case and 15 damage samples. Its source observation SHA-256 is
`e08d7149e3f46d814d5c4a709e316cf3063208bb9673141effe6b1958f03fc79`
and its production trace SHA-256 is
`219be7ec53b035dfb2e3aa35d2aeea0903551a2047340be005d185df944bc4ca`.
All three domains cover 27 cases and, with the replay gate, complete in about
0.305 seconds on Windows and 0.310 seconds in WSL. The live ground pack is
0.128 seconds warm and 2.801 seconds end to end. Position is explicitly
excluded from this damage domain because its attacker choreography and stage
position are not bound by the independently registered flat-floor player-push
theorem; slopes and remaining collision response need their own live routes
before their goldens can be admitted.

The current comparison passes 8,675 identical input frames covering held
dash/run, complete run turnaround and post-turnaround lockout, released dash
and run brake, direct dash dancing, moving dashbacks, two-sample dash
recognition, smash and empty pivots, basic standing turn including its
second-frame taunt/facing order, slow-stick sweep,
shield/light shield and defensive escapes, jump/air movement/landing, and
Falcon's complete full-down crouch start/hold/release sequence, exact and
just-beyond entry/release threshold samples, jump interruption from every
crouch state, held-crouch opposite dash/turn, crouch-release walk, and fresh
digital guard and fresh taunt from every crouch state, including Falcon's
complete 60-frame taunt duration, and first-legal-frame normal-Landing taunt,
jump, dash/turn, guard, walk, direct crouch, and ordinary turn plus the
one-frame-late down-input lockout. It additionally covers main-stick tap-jump
full/short hop, aerial jump, Landing and shield entry, threshold boundaries,
slow-sweep age rejection, and two-sample in-window recognition.

The grounded-loop geometry domain adds all 158 `SquatWait` poses and both
distinct 60-frame `AppealR`/`AppealL` tracks, for 278 poses and 3,058 capsules.
Two independent headless/unlimited captures have different raw file hashes but
the same qualified semantic SHA-256
`3c72296c3c1558d7df32228892f5b1adec4b4370e72e4d415fdc981cd2aa3ed3`.
Six physical stored controls bind the production accessor under SHA-256
`1a83c02e310c097ad609dd2e18787bec2b6589fc76f5daa039578b8e04f6a81f`.
Production now retains the decomp-driven WalkSlow/Middle/Fast and Run source
submotion, fractional animation cursor, and rate, including gait-phase remap.
Because these poses cannot use a static displayed-frame lookup, a shared
deterministic Q16 HSD evaluator consumes the pinned compact source joint/FObj
data. Independent source evaluation agrees in each of two live captures for 51
WalkSlow, 31 WalkMiddle, 29 WalkFast, and 20 Run samples (1,441 capsules)
within 2 Q16 units; eight stored observations protect the production C
evaluator. The WalkFast route enters Walk below the dash threshold, then raises
the stick above Falcon's fast-gait velocity boundary.
The expanded route also covers common RunBrake IASA and crouch common-IASA
entry: neutral A from all three crouch states; neutral B accepted only from
`Squat`; and physical Z selecting `Catch` from `Squat` but `Attack11` from
`SquatWait`/`SquatRv`. Because fighter attacks and specials remain original
content, those routes compare semantic eligibility at entry, skip the
character-specific action body, then resume exact comparison at the next
stationary anchor. Down-special entry from all three crouch states is also
covered with its character-specific bodies isolated at the end of the corpus.
A separate 348-frame Battlefield capture covers ordinary jump-through and
platform landing plus the final crouch platform-pass route. The neutral jump
retains Dolphin's final airborne crossing before `Landing`; one-frame down
release does not drop; held down exposes `Squat` frames 1-3, enters `Pass`
frame 0 at 0.63 downward speed, and lands on the solid floor on the
executable's frame.
A separate 540-frame Final Destination Falcon-versus-Falcon capture drives
grounded approach from both controller ports and directions, comparing both
fighters' action/state, action frame, facing, grounded state, position, and
self-induced velocity. Its position gate reports a 2,692-Q16 bound: the
ordinary 640 float-to-Q16.16 envelope plus one mapped 0.3-unit push nudge for a
one-tick strict-boundary transient. The other captures retain the 640-Q16
position gate; action, facing, velocity, and applicable action ticks use their
tighter independent gates. This remains a regression slice, not evidence that
the whole shared simulation has completed the binding equivalence gate.

A separate 500-frame Final Destination analog-shield capture covers both sides
of the common dead zone, four accepted pressure bands across both shoulder
inputs, simultaneous shoulders, digital full shield, release, and regeneration.
It compares action/state, shield health, and normalized pressure; the pressure
gate accepts only one unit of 16-bit conversion error.

Separate 270- and 2,158-frame memory-probed shield-geometry captures compare
the simulator against live GALE01 guard magnitude, biased guard angle, shield
joint world center, and transformed sphere radius. The sweep covers all eight
45-degree animation keys and intermediate linear samples. The comparator
accounts explicitly for the controller/post-frame pipeline offset, maps the
Melee sphere into the project's independent horizontal and vertical units,
and stops on angle, magnitude, center, or radius divergence outside the small
fixed-point gates.

Three separate 283-frame physical shield-hit captures request light,
intermediate, and dense pressure. They compare both fighters' discrete state,
facing, grounded state, position, self velocity, shield health/pressure,
hitlag, shield stun, and the attacker recoil inferred independently from the
executable's position delta minus self velocity. Component gates are exact or
32 Q16 units; position retains the established 640-Q16 conversion envelope.
Their SHA-256 values are
`563cabf633126656b80a0351b67fdffb35f664774e052e85c04ff7b20fd2e4f5`,
`84b462f717074b2a2984b6901ed33a2abd2b9f98527f1c52db400c98ace411ab`,
and `2d95549b7ffe6ac950c339fe9dcd346b4e6c401324d2cce0e8414d2677a3489f`.

A separate 271-frame Falcon jab capture reads the source fighter's live
hitboxes and the target's ordinary damage state. All three active hitboxes
report damage 2, angle 80, KBG 100, set weight 20, and BKB 0. At zero percent,
the target receives three hitlag frames and 13 hitstun frames; captured Melee
velocity is -0.17242245 x and 0.97785604 y before the documented coordinate
conversion. Capture schema 8 SHA-256 is
`2660274136b77aef393db391c85582be7795bee7360ebd6607325e437ac9af04`.

A separate 543-frame Jab 1 interruption capture pulses jump and guard one
displayed frame before and exactly on imported IASA frame 16, then holds a
horizontal stick through the same boundary. Dolphin keeps frame-15 jump and
both guard pulses in `Attack11`, starts `KneeBend` from the frame-16 jump,
enters Walk from the pre-held stick, retains displayed frame 21, and returns to
Wait on the next tick. The identical-input comparison passes with the ordinary
640-Q16 position envelope. Capture SHA-256 is
`d17f8e9a7dfc3c1a0d260d3ffe3c7fd9c3e2b5f89f3f17b1c3ce9e7218a8f427`.

A separate 329-frame defense-state capture has SHA-256
`d9dfebcb6e42f5e71ece08490429b61083f81bee067def379b5fdd6270d96b95`.
It qualifies forward roll, spot dodge, backward roll, and a held-L/fresh-R
upward air dodge through ordinary-physics handoff and floor landing, plus a
horizontal special landing that crosses the above-walk/ordinary friction
threshold. Its same-binary control SHA-256 is
`d78abcfe3d252d0f87409aba3343cd838efb739d6311494d520f2f076eb5255f`.
The comparator now checks invulnerability as an exact discrete field in addition
to action, tick, grounded state, facing, velocity, and bounded position. This
route exposed and removed duplicate backward-roll displacement, decoded the
EscapeAir frame-30 command-variable gate, and replaced generic air-dodge floor
extent with the captured 48-frame ECB bottom.
It also removed a parallel authored LandingFallSpecial displacement: the
source submotion has no TransN, and the decomp callback applies only ground
friction and post-friction ground movement.

A separate repeated 500-frame passive shield-depletion route now qualifies
Falcon's ShieldBreakFly/DownD/StandD/Furafura sequence. The full 198-frame
animated ECB catalog controls the source contact update; the comparison also includes
the one-row retained landing vertical velocity, global non-shield regeneration,
Furafura's per-update reset, independent 100-frame animation clock and exact
99-to-0 wrap, facing, grounded state, invulnerability, and shield strength.
Action/state fields are strict; position, velocity, and shield health
retain only their documented Q16 conversion envelopes. Hit-induced shield
break is separately protected in the native combat suite because its source
entry health is the common reset value rather than passive depletion's zero.

Across the main and isolated corpora, the current aggregate executable-oracle
evidence is 19,237 qualified frames. The memory-probed routes qualify the
sampled Falcon shield tilt and geometry surface; they do not qualify broader
uncaptured pressure/time/spacing routes or the other partial/divergent systems
listed above.

Current special-move geometry addendum: Falcon Dive start/catch/throw and
common FallSpecial neutral/forward/back now use the same source-derived HSD ECB
evaluator as Raptor Boost; the older eight-value FallSpecial bottom description
in the summary row is historical. Nine retained captures qualify 733 source
rows / 715 unique frames within two Q16 units, and a refreshed natural Falcon
Dive miss route passes all 165 identical-input frames. Target-switch updates
retain the decomp's install blend plus advanced-target blend, while stable
updates apply only the latter. The authored Raptor, Dive, and FallSpecial ECB
arrays are removed.

Current aerial-attack geometry addendum: Nair/Fair/Bair/Uair/Dair now extend
the shared allocation-free DAT/HSD evaluator rather than consuming the older
packed 195-value bottom table. The 25-route source qualification covers 2,066
paired Dolphin observations / 1,840 unique frames at maximum one-Q16 error for
each selected component. Bottom ownership follows the independent shared
decomp path: `ftCommon_8007D5D4` installs lock 10, map processing decrements it
before collision, and collision retains the previous desired bottom until the
lock expires. A production double-jump/Nair lifecycle gate protects entry,
frames 1-8, and frame-9 release. Rapid Jab Start is now closed by the generated
submotion animation-flag rule: motions without translation extraction remain
model-root-relative, while flagged motions subtract the animated TransN joint.

Current grounded-damage addendum: a registered two-case/60-row Hyrule line-36
theorem now qualifies the decomp's grounded slope angle test, tangent
projection, raw ground-knockback scalar, ground-origin ten-update ECB lock,
same-update post-hitlag floor recontact, Landing entry-vector retention, and
next-update grounded projection. Opposite actual Forward Tilts produce the
grounded and airborne branches from one crouch-cancelled setup. Both live
captures have identical observations; source/production semantic digests are
`657b816faa98658d10be6783b912a380cf88c24ccc1120d0a5836f61e6aa6ac9` /
`15b3705d0c7a6e9c83d3a540c6b90da4af835676011a2726fdb360a3e8fdf05e`.

Current floor-response ownership addendum: the four-case/804-row Final
Destination live oracle now distinguishes all `ftCo_80090184` floor branches.
Directional tech retains the complete airborne knockback vector with a zero
ground scalar on entry; neutral tech and missed-tech DownBound initialize the
scalar and project immediately. Every route applies ground decay and tangent
projection on its following update. Production checks the otherwise
unexposed scalar, and the shared basic-Landing path retains both incoming
knockback components with a zero scalar on its entry row.
This closes the represented line-36 slope-damage branch; wider attack, floor,
and stage combinations remain outside the claim.

Current non-tumble floor-selector addendum: generated common data now includes
the exact `ftCo_Damage_Coll` x1E0/x1E4 thresholds (5.0/0.5), and reference
production preserves the selected Damage submotion after hitstun release. The
existing Hyrule pack is four cases / 120 rows: actual low-speed Falcon Jabs
land below 0.5 while remaining DamageN2. One runs through its sourced terminal
frame and enters Wait; the other presses B on released grounded Damage frame
15 and enters Falcon Punch through `ftCo_Wait_IASA`. Two fresh captures share
source semantic SHA-256
`2ad67d79ef1fa278e5ea55096b663b0e59793167161eedc870e4c7663fe7a6a5`;
matched production SHA-256 is
`cb0b203a0a211baa55b800cd9e0cf0eb8e4595eaa069c8e865369cad8c94de61`.
The 5.0 DownBound boundary is protected by exact native threshold tests because
ordinary actual-input knockback reaches tumbling routes before exposing that
non-tumble branch. Wider attack, stage, and interrupt combinations remain open.

Current common-special acquisition addendum: the decomp's callback lists are
now represented directly rather than treating every interruptible common
state as having all four specials. `SquatWait` and `SquatRv` accept only
Falcon Kick, including a radial-gate diagonal whose side component has no
callback. `Turn` accepts Raptor Boost, Falcon Dive, and Falcon Kick while a
neutral B edge leaves Turn active. Dash exposes only SpecialS through its
x4C boundary; rejected neutral/down B retain Dash, while rejected up-B falls
through to the running tap-jump callback. A sixteen-case/188-row actual-input
Dolphin
domain and the generic native-CSV stored runner match on action, action clock,
facing, and grounded state under source/production SHA-256
`8fbfbcb12c5cdb483891315a4dc4c57a642c28ae2eb8ad886b31fecf9d3cd03d` /
`8fbfbcb12c5cdb483891315a4dc4c57a642c28ae2eb8ad886b31fecf9d3cd03d`.
The added Walk/Wait cases prove opposite Catch/Special ordering and prevent
generic projectile/charge/reflector frontends from consuming Falcon's B edge.
The reusable direct comparator also closed Turn-special facing. This closes
those callback surfaces only; unrepresented common and
character-state callback lists remain subject to the continuing source audit.

The aerial neutral-special input-history addendum closes the common
`SpecialAirN` turnaround predicate. Pinned/current decomp checks the retained
horizontal direction and strict `x676_x < x224` age before entering the
character neutral special. The generated common-data import owns `x224 == 20`;
production retains the direction and saturating age in canonical rollback
state. Three natural jump/flick/neutral/B cases compare the last accepted age
19, first rejected age 20, and a fresh same-direction negative control. Two
byte-identical 91-row captures have raw SHA-256
`3d4bb6c4a7cde8d2879e846eecf7e2fc3ca0d5151eb466fc7760678c83f58ad9`;
source and production match exactly on action, clock, facing, and grounded
state at canonical SHA-256
`027fad335436a97393260b553019fe6247661b3ae1c03d981b4b1db4cc4d5fcb`.
The causal input history is recorded and replayed through generic manifest
pre-edge phases rather than hidden in a source-only setup.

The teeter addendum closes two more source callback surfaces. Pinned and
current `ftCo_Ottotto_IASA` invoke all four common special dispatchers, while
`ftCo_OttottoWait_IASA` delegates directly to the same function. Production's
single TEETER action now owns that full capability mask for both animation
phases. A four-case/28-row actual-input Dolphin domain enters Ottotto through
an unrecorded low-stick endpoint approach, then matches neutral/side/up/down B
on action, action clock, facing, and grounded state. The two raw captures are
byte-identical at SHA-256
`a21b615da3f45642278ce4a1b2f6ba8335588e2568e423b2467fd1d55119bcca`;
source/production semantic SHA-256 values are
`2065e789ba0285f8b3d878bdc2615bf0a7e983ee02da356f6f46d0b924a6908e` /
`2065e789ba0285f8b3d878bdc2615bf0a7e983ee02da356f6f46d0b924a6908e`.
The direct comparator exposed and closed both Teeter up-special eligibility
and Falcon Punch's `ft_800827A0` mode-2 endpoint clamp. The fixture never
writes a teeter action directly. Unrepresented callback
lists remain open.

The GuardOff addendum closes its complete represented acquisition surface.
Pinned and current `ftCo_GuardOff_IASA` expose the complete common special,
attack, and grab dispatcher only when GuardOff's powershield work flag is set;
ordinary GuardOff omits it. Both branches then check spot dodge followed by
button/tap/C-stick jump. Production reuses the existing stack-local capability
mask and powershield-release predicate, so ShieldRelease gains the complete
special/attack/grab dispatcher only on the source powershield branch while
both branches retain the shared movement callbacks. A 17-case/119-row physical
Falcon-Jab domain matches all four acquired specials, jab, all three tilts,
all three smashes, grab, an ordinary-shield neutral-B negative control, and
ordinary/powershield jump and spot dodge. Its
source/production semantic SHA-256 values are
`851a0c05e393bd644344bf8a49d70fceea179727903ff68feacebb1c12a27c0d` /
`851a0c05e393bd644344bf8a49d70fceea179727903ff68feacebb1c12a27c0d`.
The direct comparator additionally fixes and protects EscapeN and ground-
attack displayed frame 1 on the GuardOff acquisition update.
The setup resets both fighters' collision-position history, begins shield on
the attack edge, and reaffirms it at the opponent's observed Jab frame. Six
concurrent shards keep each divergent worker at three cases or fewer; it is
neither a web probe nor a synthetic source-state write. Other unrepresented
  callback lists remain open.

## KneeBend common-callback acquisition

`ftCo_KneeBend_IASA` calls the misleadingly named
`ftCo_Attack100_CheckInput` first. Its implementation is not rapid-jab input:
it invokes Falcon's `ftData_SpecialHi` callback when the common up-special
lockout is clear. Catch and `ftCo_AttackHi4_CheckInputNoD0` follow, so up-B
wins over a simultaneous Z input; no other special direction is exposed from
KneeBend.

Production now expresses this as the up bit in the existing stack-local
special capability mask for `PF_M4_ACTION_JUMP_SQUAT`. The ordinary shared
special transition remains authoritative. A three-case live theorem reaches
KneeBend through a real jump edge and proves up-B, up-B+Z priority, and side-B
rejection. All 18 action/tick/facing/grounded samples match production exactly
at semantic SHA-256
`0695488cb8bff660bfabe69298f366ed7bbbfed4348330636b04f87bff43aa17`.

The same body audit closes SquatWait and SquatRv: each calls down special and
then up special, while omitting side and neutral. Production expresses this as
the up+down capability mask for Crouch/CrouchEnd. Four natural axial/diagonal
routes match source for all 106 action/tick/facing/grounded observations.

## Released Damage and DamageFall air-dodge callback

Pinned revision `9509dc04406fb2028bfab01243841ba4787c0fb7` distinguishes
two post-hitstun airborne callback tables. Ordinary non-tumble Damage reaches
the Fall IASA table through `ftCo_Damage_IASA`; that table calls the EscapeAir
checker. DamageFly/DamageFall reaches `ftCo_DamageFall_IASA`, whose sourced
special, item, tether, double-jump, and wiggle branches do not include
EscapeAir. The relevant source files are independently pinned at SHA-256
`a3852f6377a71d03736b70b3869016a437b68c17dd703faead5be2954eb0278a`
(`ftCo_Damage.c`),
`973ce744a0e1084377bef6cebdeca6631fb90a0f8a31694621e0c5052b896a8b`
(`ftCo_DamageFall.c`), and
`cdff68de39d55855f1ca02b8e4af09ce856a1133cc21b23921a881b23e0dfaf6`
(`ftCo_EscapeAir.c`).

Production now applies the existing air-dodge transition only when the
canonical tumble bit is clear. It does not add a damage-only transition,
content constant, allocation, or rollback state. The focused live pack retains
68 rows across two physical cases: fresh L enters `AIRDODGE` after ordinary
non-tumble Damage release, while DamageFly/DamageFall rejects the same edge and
remains `TUMBLING` for both retained airborne rows. Source semantic SHA-256 is
`7ce52b784989e56f7539b79dd779eed94ab41e4bcd624b980c263af0b916084b`;
production canonical SHA-256 is
`cec3d2b1d9b67ad906bf68b074c8975f6e53bf48cc714650c6498bef7aeba93e`.
Warm capture times are 0.587707 and 0.897459 seconds; the second complete
lifecycle takes 4.981875 seconds.

The fast stored theorem keeps only the four discrete action/tumble samples
needed to protect this callback split. The resulting 29-domain / 170-case gate
plus replay passes in 1.286 seconds on Windows and 1.188 seconds in WSL under
manifest SHA-256
`b4406686d48f9bcc8719d89f558a246dad05d25d5cb8362cde4b64d093aa0be2`.
Windows serial CTest passes 40/40 in 8.42 seconds and WSL passes 42/42 in 9.82
seconds. This closes only the represented released-air-dodge distinction;
uncaptured damage callbacks and interactions remain outside the claim.

## Run-to-RunBrake acquisition

Pinned decomp revision `9509dc04406fb2028bfab01243841ba4787c0fb7` and
current upstream `d882af94175e3c880ad51039e2979aa9a50aea09` are represented
by four natural input routes. From ordinary Run, straight full down enters
RunBrake while a radial-gate diagonal-down edge remains Run for its first row.
Once RunBrake is active, straight down enters CrouchStart. A separate
full-opposite TurnRun setup reaches its locked Run phase and proves that the
same down edge is rejected there. This supersedes the simulation's former
direct Run-to-Crouch behavior; it does not broaden the claim to uncaptured Run
or TurnRun callbacks. Normalized source SHA-256 values are
`72a9ce8c19948d468f6aea484b72db3b1f0c280846adc4d5677e4c6a20b810fe`
for `ftCo_Run.c`,
`0c75e6a95319f2be3a42dcade65b07671d47d7a31e7191e04cb617fce13866bb`
for `ftCo_RunBrake.c`, and
`80c2e71e50622e942754bfcdd3bd89f3762fe4df2400d8055f059ab6cc4b8082`
for `ftCo_Squat.c`.

Production removes Run from the generic direct-crouch predicate and therefore
falls through to the existing shared RunBrake transition. The existing
RunBrake-to-crouch route remains the sole represented sliding crouch entry.
This adds no content constant, action-specific router, allocation, lookup
table, canonical field, save field, or snapshot byte.

The live pack retains 127 rows across four immutable checkpoint slots. Two
captures have identical ordered rows and distinct raw artifact SHA-256 values
`1d3c568f38f6dcd359e77c3b1616a6e7d81480dff4e8b3aa5262e528533fd8b9`
and
`e74a8c0ecc7628ba2886e7ad10b4633d2e1ad0eac5ecf6c5ec86f057a9d1ab16`.
Warm capture work takes 0.541609 and 0.311504 seconds; complete lifecycles take
6.177062 and 3.647950 seconds. The selected source and production
action/tick/facing/grounded payloads are structurally identical at SHA-256
`dfa7be0339110c98c9107a069ef7e9751b14f2c174bd04a7e977c90ae745f6ad`.

The reusable native-CSV stored domain keeps all four cases / 127 samples and
passes by itself in 263.089 ms on Windows and 403.007 ms in WSL. The 30-domain /
174-case registry plus replay passes in three isolated runs per platform:
1178.830/1319.197/1471.076 ms on Windows and
919.397/986.464/1270.306 ms in WSL under manifest SHA-256
`99b5f633b2f4f6c33173ca285af0634e0ac51d1acc6df8b2a5b3c57f22cb261d`.
