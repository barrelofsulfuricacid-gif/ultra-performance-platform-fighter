# Plan modifications

## 2026-08-03 - M4 SSBM frame-data fidelity exception

The owner explicitly authorized importing frame-data tables while requesting
that movement and simulation behavior match the SSBM decomp. This overrides
the M0 wording that required independently authored implementation data and
prohibited imported frame data, but only for numeric gameplay behavior and
state-machine research.

The exception does not authorize importing or shipping executable game code,
the disc image, extracted archives, names, art, animation, audio, stage
geometry, writing, UI expression, or other assets. Every imported value must
have a pinned source revision or user-owned NTSC 1.02 extraction record, an
explicit coordinate conversion where needed, and a field-level provenance
entry. Extracted files remain outside the repository.

This materially changes the project's originality/IP posture. The existing
formal pre-release IP review remains mandatory and must now specifically cover
the imported gameplay tables and the degree of behavioral equivalence.

M4 validation will distinguish `equivalent`, `partial`, `divergent`, and
`missing` behavior. A passing deterministic test proves the implemented
contract; it does not by itself prove complete SSBM equivalence.
