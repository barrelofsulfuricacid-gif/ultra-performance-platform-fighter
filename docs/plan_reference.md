# Governing plan

The governing artifact is
`ultra_performance_platform_fighter_implementation_plan.md`, current living
revision as of 2026-08-04, SHA-256:

`c89851b1df4bccb1fd17ff4c3d10aa4e0b5e919f7bcc267626e8f7153d9ef160`

The owner resolved the binding choices on 2026-07-27:

- D1-A: mechanical counterpart for every playable SSBM fighter/form.
- D2-A: authored game code in C; required C++ dependencies behind C ABIs.
- D3-C: reproducible relative improvement and non-regression only.
- D4-A: full native/web cross-play.
- D5-A: P2P ranked gameplay with server replay verification.
- D6-A: runtime Excel import for authoring and validated production packs.

On 2026-08-04 the owner added a cross-cutting acceptance gate: correct and
near-SSBM-equivalent behavior must also be beautifully implemented with the
correct zero-cost abstractions, one canonical authority for gameplay policy,
and minimal-to-nonexistent logic duplication. The governing plan and
`plan_modifications.md` define the measurable consequences.

This repository must not silently reinterpret those choices. A material change
requires an entry in `plan_modifications.md`.
