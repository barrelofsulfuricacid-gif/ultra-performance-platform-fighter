# Governing plan

The governing artifact is
`ultra_performance_platform_fighter_implementation_plan.md`, version 1,
SHA-256:

`aceed9eccc03035bf488f47a76b5794880856958fbea182c815120afbd307abb`

The owner resolved the binding choices on 2026-07-27:

- D1-A: mechanical counterpart for every playable SSBM fighter/form.
- D2-A: authored game code in C; required C++ dependencies behind C ABIs.
- D3-C: reproducible relative improvement and non-regression only.
- D4-A: full native/web cross-play.
- D5-A: P2P ranked gameplay with server replay verification.
- D6-A: runtime Excel import for authoring and validated production packs.

This repository must not silently reinterpret those choices. A material change
requires an entry in `plan_modifications.md`.
