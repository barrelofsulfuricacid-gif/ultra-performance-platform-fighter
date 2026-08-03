# Originality and provenance contract

## Scope and caution

This is an engineering/content control, not legal advice. D1-A intentionally
targets close functional counterparts, which creates elevated IP risk even when
all shipped expression is original. Qualified counsel must review the game
before public announcement or release.

Official guidance distinguishes unprotected ideas, systems, and methods from
protected expression, while video games can contain protected software,
audiovisual, musical, literary, trademark, and other elements. The project
therefore separates behavioral requirements from all expressive production.

## Prohibited inputs

The 2026-08-03 owner-approved M4 exception in `plan_modifications.md` permits
numeric gameplay frame tables and formulas for SSBM behavioral fidelity when
they are recorded in a field-level provenance document. The exception does not
permit any expressive asset or executable game code, and extracted source
files must stay outside this repository. Where the bullets below mention frame
tables, formulas, or decompilation, that narrow recorded exception controls.

Do not place any of the following in the repository, build pipeline, design
workbooks, model-training data, reference boards, or shipped product:

- Ripped or traced Nintendo/Supergiant/third-party sprites, textures, models,
  animations, UI, fonts, icons, effects, video, audio, music, or voice.
- Decompilations, disassemblies, leaked source, extracted formulas, copied frame
  tables, memory dumps, hitbox dumps, collision geometry, or proprietary file
  formats used to reproduce implementation data.
- Nintendo character, stage, item, franchise, or move names as shipped names.
- Logos, title treatments, menu compositions, sound logos, controller glyphs,
  trade dress, or marketing that could imply affiliation or endorsement.
- Redrawn or transformed copies that retain recognizable protected character or
  environmental expression.
- Prompts that request a living artist’s or studio’s exact style, named
  character, composition, costume, or asset.
- Unlicensed fan art, remixes, samples, voice clips, or marketplace assets.

Internal requirement documents may use reference fighter names only to map the
D1-A mechanical coverage obligation. Runtime data and public-facing content use
original identifiers.

## Clean behavioral specification

Reference analysis records only:

- Input/action category.
- Functional purpose in neutral, advantage, disadvantage, recovery, or teams.
- Broad timing class and relative risk/reward.
- Movement/attack relationship.
- Matchup identity and intended counterplay.
- Emergent technique that must remain possible.

Implementation then independently authors:

- Numeric formulas and constants.
- State-machine structure.
- Collision/hitbox geometry.
- Frame counts and cancel windows.
- Fighter proportions and locomotion.
- Move choreography and visual effects.
- Names, terminology, story, and presentation.

No blanket claim of exact frame-perfect Melee reproduction is made. Individual
systems may be marked equivalent only after decomp comparison and deterministic
verification; all remaining gaps stay explicit in the fidelity audit.

## Art-direction separation

The visual target is described through general qualities:

- Hand-painted 2D surfaces.
- Strong silhouette readability.
- High-contrast value grouping.
- Graphic line accents.
- Dramatic but legible color lighting.
- Expressive key poses and economical smear effects.

The art bible must establish an original world, shape language, palette system,
materials, anatomy, costume logic, camera treatment, UI grammar, and animation
principles. It must not reproduce *Hades* characters, environments, UI,
composition, palette signatures, brushwork samples, or iconography.

## Audio and music separation

- Every composition begins from an original brief describing mood, tempo,
  instrumentation, loop structure, and interactive layers.
- No melody, harmony sequence, arrangement, sample, voice, or sound effect is
  copied from SSBM or another game.
- Temporary reference audio is never imported into the build.
- Third-party samples require a license covering commercial interactive use,
  modification, redistribution inside the game, and every target platform.
- Performer, composer, and sound-designer agreements define ownership and
  credit.

## Naming and trademark gate

Before approving a game title, fighter name, mode name, service name, logo, or
major fictional organization:

1. Search relevant Canadian, United States, and intended-market trademark
   databases.
2. Search game storefronts, domains, social handles, and ordinary web use.
3. Record candidates, search date, classes/categories, confusingly similar
   results, and reviewer.
4. Obtain legal review before filing or public use.

No public title is selected during M0.

## Provenance record

Every asset or content bundle must have a machine-readable record containing:

- Stable asset ID and content hash.
- Human-readable title and type.
- Creator/legal owner.
- Creation date and toolchain.
- Source file and exported derivative paths.
- Whether generative tools were used and the retained human-authored process.
- References used, with confirmation that no prohibited reference was supplied
  to generation.
- License/agreement identifier and permitted uses.
- Attribution requirements.
- Reviewer and review date.
- Replacement/retirement history.

Unknown provenance fails the build once the asset pipeline is enforced.

## Review gates

### Per asset

- Original source and working files exist.
- Provenance is complete.
- No prohibited name, logo, silhouette, melody, animation, or UI composition is
  recognizable.
- License obligations are compatible with native, web, marketing, streaming,
  and commercial distribution.

### Per fighter/stage wave

- Mechanical-reference notes contain behavior only.
- Character/world expression is independently authored.
- Side-by-side review checks overall impression, not just pixel identity.
- All third-party contributions have signed rights/assignment terms.

### Before announcement/release

- Counsel reviews D1-A counterpart risk, title/marks, roster, stages, UI, art,
  audio/music, marketing, dependency licenses, contributor agreements, and
  storefront materials.
- Required trademark searches are refreshed.
- No unresolved high-risk provenance issue remains.

## Official research basis

- [U.S. Copyright Office: copyright protects expression, not ideas, systems, or methods](https://www.copyright.gov/help/faq/faq-protect.html)
- [17 U.S.C. § 102(b)](https://www.copyright.gov/title17/92chap1.html)
- [U.S. Copyright Office Circular 33: works not protected](https://www.copyright.gov/circs/circ33.pdf)
- [U.S. Copyright Office: video games as audiovisual works](https://www.copyright.gov/registration/motion-pictures/)
- [Canadian Intellectual Property Office: protecting IP in video games](https://ised-isde.canada.ca/site/canadian-intellectual-property-office/en/corporate-information/canadian-ip-voices-podcast-case-studies-and-blog/guard-your-games-protecting-intellectual-property-video-games)
- [Canadian Intellectual Property Office](https://ised-isde.canada.ca/site/canadian-intellectual-property-office/en/canadian-intellectual-property-office)
- [USPTO trademark search guidance](https://www.uspto.gov/trademarks/search)
