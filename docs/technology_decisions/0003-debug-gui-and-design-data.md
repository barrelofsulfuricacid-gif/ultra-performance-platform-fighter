# TDR-0003: Debug GUI and design-data ingestion

- **Status:** Accepted for M1/M5 spikes
- **Date:** 2026-07-27
- **Implements:** D2-A and D6-A

## Debug GUI decision

Use Dear ImGui through the generated Dear Bindings C API. Pin the first spike
to the mutually matched pair:

- Dear ImGui 1.92.8.
- Dear Bindings 0.21 / `DearBindings_v0.21_ImGui_v1.92.8`
  (`c9ff649`).

Dear ImGui 1.92.9 was current during research, but the selected generated
binding release targets 1.92.8. Matching generated code is safer than mixing
versions. Both projects use the MIT license.

The C adapter exposes project-specific debug operations rather than making the
entire generated API a dependency of gameplay code. Dear ImGui and its C++
implementation are absent from release headless builds.

Evidence:

- [Dear ImGui releases](https://github.com/ocornut/imgui/releases)
- [Dear Bindings 0.21 / ImGui 1.92.8 release](https://github.com/dearimgui/dear_bindings/releases/tag/DearBindings_v0.21_ImGui_v1.92.8)
- [Dear ImGui binding guidance](https://github.com/ocornut/imgui/wiki/Bindings)

## Workbook decision

Use XLSX I/O 0.2.36 (`a9016eb`, MIT) as the first native C reader candidate.
Compare it with a deliberately small schema-specific ZIP/XML value reader
before M5 adoption.

XLSX I/O depends on XML/ZIP components, so it never enters `sim`, web release,
headless release, or a per-tick path. The authored importer maps workbook
values into one canonical model; authored validation and pack emission remain
library-independent.

Evidence:

- [XLSX I/O 0.2.36 release](https://github.com/brechtsanders/xlsxio/releases/tag/0.2.36)
- [XLSX I/O project and license](https://github.com/brechtsanders/xlsxio)

## Acceptance

- Generated GUI headers compile cleanly from C and no C++ type crosses the
  project adapter.
- The GUI can be removed at compile/link time.
- Both workbook candidates ingest the same fixture corpus and produce
  byte-identical canonical packs or identical diagnostics.
- Import rejects formulas-without-cached-values, duplicate IDs, unknown
  references, invalid ranges, capacity overflow, NaN/infinity, locale-dependent
  numerics, and unsupported schema versions.
- Runtime developer import and offline production packing share validation and
  conversion code.

## Replacement plan

Regenerate or replace the GUI adapter without changing debug feature call
sites. Replace XLSX I/O behind a row/cell value stream; the canonical model,
validator, and packer remain unchanged.
