#!/usr/bin/env python3
"""Small, strict readers shared by SSBM DAT importers."""

from __future__ import annotations

from dataclasses import dataclass
import struct


@dataclass(frozen=True)
class HsdArchive:
    """The immutable data block and named roots of one HSD archive."""

    data: bytes
    roots: dict[str, int]
    references: dict[str, int]

    def root(self, name: str) -> int:
        try:
            return self.roots[name]
        except KeyError as error:
            raise ValueError(f"missing HSD archive root {name!r}") from error


def _archive_name(raw: bytes, string_table: int, relative_offset: int) -> str:
    start = string_table + relative_offset
    if start < string_table or start >= len(raw):
        raise ValueError("HSD archive name offset is out of bounds")
    end = raw.find(b"\0", start)
    if end < 0:
        raise ValueError("unterminated HSD archive name")
    try:
        return raw[start:end].decode("ascii")
    except UnicodeDecodeError as error:
        raise ValueError("non-ASCII HSD archive name") from error


def read_hsd_archive(raw: bytes) -> HsdArchive:
    """Decode and validate the common HSD archive header/root tables.

    Offsets in the root and relocation tables are relative to the data block
    beginning at archive offset 0x20. Relocations are validated here even when
    a caller only needs named roots, so truncated or malformed source input
    fails before character-specific decoding begins.
    """

    if len(raw) < 0x20:
        raise ValueError("truncated HSD archive header")
    (
        file_size,
        data_size,
        relocation_count,
        root_count,
        reference_count,
    ) = struct.unpack_from(">5I", raw, 0)
    if file_size != len(raw):
        raise ValueError(
            f"HSD archive size mismatch: header={file_size} actual={len(raw)}"
        )
    data_end = 0x20 + data_size
    relocation_end = data_end + relocation_count * 4
    root_end = relocation_end + root_count * 8
    reference_end = root_end + reference_count * 8
    if not (0x20 <= data_end <= relocation_end <= root_end <= reference_end <= len(raw)):
        raise ValueError("HSD archive tables are out of bounds")

    data = raw[0x20:data_end]
    for relocation_index in range(relocation_count):
        relocation = struct.unpack_from(
            ">I", raw, data_end + relocation_index * 4
        )[0]
        if relocation + 4 > len(data):
            raise ValueError(
                f"HSD relocation {relocation_index} is out of bounds"
            )

    string_table = reference_end

    def named_entries(offset: int, count: int, kind: str) -> dict[str, int]:
        entries: dict[str, int] = {}
        for index in range(count):
            data_offset, name_offset = struct.unpack_from(
                ">2I", raw, offset + index * 8
            )
            if data_offset >= len(data):
                raise ValueError(f"HSD {kind} {index} data offset is out of bounds")
            name = _archive_name(raw, string_table, name_offset)
            if name in entries:
                raise ValueError(f"duplicate HSD {kind} name {name!r}")
            entries[name] = data_offset
        return entries

    return HsdArchive(
        data=data,
        roots=named_entries(relocation_end, root_count, "root"),
        references=named_entries(root_end, reference_count, "reference"),
    )


def ft_common_data(raw: bytes) -> tuple[bytes, tuple[int, ...]]:
    """Return PlCo.dat's data block and complete ftLoadCommonData pointers."""

    archive = read_hsd_archive(raw)
    if archive.references:
        raise ValueError("unexpected PlCo.dat reference table")
    if set(archive.roots) != {"ftLoadCommonData"}:
        raise ValueError("unexpected PlCo.dat root table")
    root_offset = archive.root("ftLoadCommonData")
    pointer_count = 23
    if root_offset + pointer_count * 4 > len(archive.data):
        raise ValueError("truncated ftLoadCommonData pointer table")
    pointers = struct.unpack_from(
        f">{pointer_count}I", archive.data, root_offset
    )
    if any(pointer >= len(archive.data) for pointer in pointers):
        raise ValueError("ftLoadCommonData pointer is out of bounds")
    return archive.data, pointers
