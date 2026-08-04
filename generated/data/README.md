# Generated game data

Validated, versioned content packs generated from `design/workbooks/` live
here. A pack must record its schema version, source-workbook hash, generator
version, and content hash.

Release, web, and headless products consume these packs; developer-only
runtime workbook import remains an explicit diagnostic path.

`m4_falcon_ntsc102_frame_data.inc` is the owner-authorized numeric exception:
all 50 Falcon NTSC 1.02 attack subactions converted by
`tools/import_ssbm_falcon_frame_data.py`. Its pinned source hashes and tool
revisions are recorded in
`docs/product/m4_falcon_ntsc102_data_provenance.md`; no extracted DAT or
hitbox-geometry dump is tracked.
Production Falcon-counterpart code indexes this generated table rather than
duplicating numeric move constants in hand-authored defaults.
