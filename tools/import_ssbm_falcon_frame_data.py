#!/usr/bin/env python3
"""Convert the pinned Falcon NTSC 1.02 frame-data dump to compact C tables."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
from typing import Any

from hsd_figatree import TRACK_TRANSLATE_Z, decode_figatree, sample_track


EXPECTED_CANONICAL_SHA256 = (
    "42bb4ecefb33e87dc978482ecdb7b1f93ff12ca090e870431fff913480601356"
)
EXTRACTOR_REVISION = "0b12c5cb988da3fb9b67630b1d8347e12cd91528"
DAT_READER_REVISION = "d4e6074aa26f388fccc7fe8e825761cf1c1bc7b0"
HSD_READER_REVISION = "29546ad77fdf9ebd9a9940ed44903ef309e810d6"
SOURCE_DAT_SHA256 = (
    "4cf61a52737d464df9298fd15573345fb3b9a15c79ab47dce4fd2e3e707917af"
)
SOURCE_ANIMATION_DAT_SHA256 = (
    "a9a0ccc2382a2f02d5423675469719488540dd119a14577712c97348f70e1c1a"
)
SOURCE_DAT_JSON_SHA256 = (
    "fa18647a5d94826429ef6f961461e66118dcb18e0a30fa124d1bbf03c6476266"
)

MOVE_KEYS = (
    "jab1", "jab2", "jab3", "rapidjabs_start", "rapidjabs_loop",
    "rapidjabs_end", "dashattack", "ftilt_h", "ftilt_mh", "ftilt_m",
    "ftilt_ml", "ftilt_l", "utilt", "dtilt", "fsmash_h", "fsmash_mh",
    "fsmash_m", "fsmash_ml", "fsmash_l", "usmash", "dsmash", "nair",
    "fair", "bair", "uair", "dair", "grab", "dashgrab", "pummel",
    "fthrow", "bthrow", "uthrow", "dthrow", "0x12d", "0x12e", "0x12f",
    "0x130", "0x131", "0x132", "0x133", "0x134", "0x135", "0x136",
    "0x137", "0x138", "0x139", "0x13a", "0x13b", "0x13c", "0x13d",
)

# The extractor uses the common five-angle forward-smash schema. Falcon's DAT
# has only high, straight, and low variants, so these two slots are
# intentionally absent rather than incomplete extraction results.
EXPECTED_ABSENT_MOVE_KEYS = frozenset({"fsmash_mh", "fsmash_ml"})

ELEMENTS = {
    "empty": "PF_M4_REFERENCE_HIT_EMPTY",
    "normal": "PF_M4_REFERENCE_HIT_NORMAL",
    "fire": "PF_M4_REFERENCE_HIT_FIRE",
    "electric": "PF_M4_REFERENCE_HIT_ELECTRIC",
    "grab": "PF_M4_REFERENCE_HIT_GRAB",
}

ANIMATION_TRANSLATION_FLAG = 0x80000000
MELEE_X_TO_SIM_Q16 = 65536.0 * 12.0 / 115.0


def canonical_sha256(data: dict[str, Any]) -> str:
    encoded = json.dumps(
        data, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def u16(value: Any) -> int:
    return 0 if value is None else int(value)


def throw_release_frame(
    dat_data: dict[str, Any], subaction_index: int, subaction_name: str
) -> int:
    """Read the release flag's exact action-script frame.

    meleeDat2Json calls opcode 0x14 (encoded first byte 0x50)
    ``reverseDirection``. The NTSC 1.02 decomp dispatches that opcode to
    ftAction_800718A4: argument zero raises throw_flags_b3, which
    ftCo_800DD724 consumes to release the victim. Argument one reverses
    facing instead. Reading the encoded argument avoids relying on the
    upstream display label.
    """
    subactions = dat_data["nodes"][0]["data"]["subactions"]
    subaction = subactions[subaction_index]
    if str(subaction["name"]) != subaction_name:
        raise ValueError(f"subaction {subaction_index}: name mismatch")

    frame = 0
    releases: list[int] = []
    for event in subaction.get("events", []):
        command_id = int(str(event["commandId"]), 16)
        fields = event.get("fields") or {}
        if command_id == 0x08:
            frame = int(fields["frame"])
        elif command_id == 0x04:
            frame += int(fields["frames"])
        elif command_id == 0x50:
            encoded = bytes.fromhex(str(event["bytes"]))
            argument = int.from_bytes(encoded, "big") & ((1 << 26) - 1)
            if argument == 0:
                releases.append(frame)
    if len(releases) != 1:
        raise ValueError(
            f"subaction {subaction_index}: expected one throw release, "
            f"found {releases}"
        )
    return releases[0]


def generate(
    data: dict[str, Any],
    dat_data: dict[str, Any],
    source_dat: bytes,
    animation_dat: bytes,
) -> str:
    phases: list[tuple[int, int, int]] = []
    effects: list[dict[str, Any]] = []
    throws: list[dict[str, Any]] = []
    moves: list[dict[str, int]] = []
    motion_x_q16: list[int] = []

    fighter_data = dat_data["nodes"][0]["data"]
    subactions = fighter_data["subactions"]
    subactions_offset = int(fighter_data["subactionsOffset"])
    attributes = {
        str(attribute["name"]): attribute["value"]
        for attribute in fighter_data["attributes"]
    }
    model_scaling = float(attributes["modelScaling"])
    source_dat_block = source_dat[0x20:]

    if tuple(data) != MOVE_KEYS:
        raise ValueError("unexpected move order or incomplete Falcon table")

    for key in MOVE_KEYS:
        move = data[key]
        if move is None:
            if key not in EXPECTED_ABSENT_MOVE_KEYS:
                raise ValueError(f"{key}: incomplete Falcon frame-data row")
            moves.append({"present": 0})
            continue
        if key in EXPECTED_ABSENT_MOVE_KEYS:
            raise ValueError(f"{key}: unexpected Falcon frame-data row")

        phase_offset = len(phases)
        effect_offset = len(effects)
        move_effects = move.get("hitboxes", [])
        if len(move_effects) > 16:
            raise ValueError(f"{key}: too many hit effects")
        effects.extend(move_effects)
        for phase in move.get("hitFrames", []):
            mask = 0
            for effect_index in phase["hitboxes"]:
                if not 0 <= int(effect_index) < len(move_effects):
                    raise ValueError(f"{key}: invalid hit-effect index")
                mask |= 1 << int(effect_index)
            phases.append((int(phase["start"]), int(phase["end"]), mask))

        throw_index = 0xFFFF
        if "throw" in move:
            throw_index = len(throws)
            throw = dict(move["throw"])
            throw["releaseFrame"] = (
                throw_release_frame(
                    dat_data,
                    int(move["subactionIndex"]),
                    str(move["subactionName"]),
                )
                if key in {"fthrow", "bthrow", "uthrow", "dthrow"}
                else 0
            )
            throws.append(throw)
        subaction_index = int(move["subactionIndex"])
        action_flags = struct.unpack_from(
            ">I",
            source_dat_block,
            subactions_offset + subaction_index * 0x18 + 0x10,
        )[0]
        motion_offset = len(motion_x_q16)
        if action_flags & ANIMATION_TRANSLATION_FLAG:
            subaction = subactions[subaction_index]
            animation_offset = int(subaction["animOffset"])
            animation_size = int(subaction["animSize"])
            tree = decode_figatree(
                animation_dat[
                    animation_offset:animation_offset + animation_size
                ]
            )
            translation_node = (action_flags & 0xFF) - 1
            if not 0 <= translation_node < len(tree.nodes):
                raise ValueError(
                    f"{key}: invalid translation node {translation_node}"
                )
            translation_tracks = [
                track
                for track in tree.nodes[translation_node]
                if track.track_type == TRACK_TRANSLATE_Z
            ]
            if len(translation_tracks) != 1:
                raise ValueError(
                    f"{key}: expected one translation-Z track, "
                    f"found {len(translation_tracks)}"
                )
            translation = translation_tracks[0]
            positions = [
                sample_track(translation, float(frame))
                for frame in range(int(move["totalFrames"]) + 1)
            ]
            motion_x_q16.extend(
                round(
                    (positions[frame] - positions[frame - 1])
                    * model_scaling
                    * MELEE_X_TO_SIM_Q16
                )
                for frame in range(1, len(positions))
            )
        moves.append(
            {
                "present": 1,
                "subaction": int(move["subactionIndex"]),
                "total": int(move["totalFrames"]),
                "iasa": u16(move.get("iasa")),
                "charge": u16(move.get("chargeFrame")),
                "autocancel_before": u16(move.get("autoCancelBefore")),
                "autocancel_after": u16(move.get("autoCancelAfter")),
                "landing": u16(move.get("landingLag")),
                "l_cancelled": u16(move.get("lcancelledLandingLag")),
                "phase_offset": phase_offset,
                "effect_offset": effect_offset,
                "throw_index": throw_index,
                "phase_count": len(move.get("hitFrames", [])),
                "effect_count": len(move_effects),
                "animation_flags": action_flags,
                "motion_offset": motion_offset,
                "motion_count": len(motion_x_q16) - motion_offset,
            }
        )

    lines = [
        "/* Generated by tools/import_ssbm_falcon_frame_data.py; do not edit. */",
        f"/* canonical source SHA-256: {EXPECTED_CANONICAL_SHA256} */",
        f"/* extractor revision: {EXTRACTOR_REVISION} */",
        f"/* DAT reader revision: {DAT_READER_REVISION} */",
        f"/* HSD animation reader revision: {HSD_READER_REVISION} */",
        f"/* PlCa.dat SHA-256: {SOURCE_DAT_SHA256} */",
        f"/* PlCaAJ.dat SHA-256: {SOURCE_ANIMATION_DAT_SHA256} */",
        f"/* PlCa.dat JSON SHA-256: {SOURCE_DAT_JSON_SHA256} */",
        "",
        "static const uint8_t pf_m4_falcon_source_sha256[32] = {",
        "    " + ", ".join(
            f"UINT8_C(0x{EXPECTED_CANONICAL_SHA256[index:index + 2]})"
            for index in range(0, len(EXPECTED_CANONICAL_SHA256), 2)
        ),
        "};",
        "",
        "static const pf_m4_reference_hit_phase pf_m4_falcon_hit_phases[] = {",
    ]
    lines.extend(
        f"    {{ UINT16_C({start}), UINT16_C({end}), UINT16_C({mask}), UINT16_C(0) }},"
        for start, end, mask in phases
    )
    lines.extend(("};", "", "static const pf_m4_reference_hit_effect pf_m4_falcon_hit_effects[] = {"))
    for effect in effects:
        element = ELEMENTS[str(effect["element"])]
        lines.append(
            "    { "
            f"UINT16_C({int(effect['angle'])}), "
            f"UINT16_C({int(effect['kbGrowth'])}), "
            f"UINT16_C({int(effect['weightDepKb'])}), "
            f"UINT16_C({int(effect['baseKb'])}), "
            f"UINT8_C({int(effect['damage'])}), "
            f"UINT8_C({int(effect['shieldDamage'])}), "
            f"UINT8_C({int(effect['hitboxInteraction'])}), "
            f"(uint8_t){element}, "
            f"UINT8_C({1 if effect['hitGrounded'] else 0}), "
            f"UINT8_C({1 if effect['hitAirborne'] else 0}), "
            "{ UINT8_C(0), UINT8_C(0) } },"
        )
    lines.extend(("};", "", "static const pf_m4_reference_throw pf_m4_falcon_throws[] = {"))
    for throw in throws:
        lines.append(
            "    { "
            f"UINT16_C({int(throw['angle'])}), "
            f"UINT16_C({int(throw['kbGrowth'])}), "
            f"UINT16_C({int(throw['weightDepKb'])}), "
            f"UINT16_C({int(throw['baseKb'])}), "
            f"UINT8_C({int(throw['damage'])}), "
            f"UINT8_C({int(throw['element'])}), "
            f"UINT16_C({int(throw['releaseFrame'])}), "
            "UINT16_C(0) },"
        )
    lines.extend(("};", "", "static const pf_m4_reference_move pf_m4_falcon_moves[PF_M4_FALCON_MOVE_COUNT] = {"))
    for move in moves:
        if move["present"] == 0:
            lines.append("    { 0 },")
            continue
        lines.append(
            "    { "
            f"UINT16_C({move['subaction']}), UINT16_C({move['total']}), "
            f"UINT16_C({move['iasa']}), UINT16_C({move['charge']}), "
            f"UINT16_C({move['autocancel_before']}), "
            f"UINT16_C({move['autocancel_after']}), "
            f"UINT16_C({move['landing']}), UINT16_C({move['l_cancelled']}), "
            f"UINT16_C({move['phase_offset']}), UINT16_C({move['effect_offset']}), "
            f"UINT16_C({move['throw_index']}), UINT8_C({move['phase_count']}), "
            f"UINT8_C({move['effect_count']}), UINT8_C(1), UINT8_C(0), "
            f"UINT32_C({move['animation_flags']}), "
            f"UINT16_C({move['motion_offset']}), "
            f"UINT16_C({move['motion_count']}) }},"
        )
    lines.extend(("};", "", "static const int32_t pf_m4_falcon_motion_x_q16[] = {"))
    lines.extend(
        "    " + ", ".join(
            f"INT32_C({value})"
            for value in motion_x_q16[index:index + 8]
        ) + ","
        for index in range(0, len(motion_x_q16), 8)
    )
    lines.extend(("};", ""))
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("dat_source", type=Path)
    parser.add_argument("source_dat", type=Path)
    parser.add_argument("animation_dat", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    data = json.loads(args.source.read_text(encoding="utf-8"))
    dat_bytes = args.dat_source.read_bytes()
    dat_digest = hashlib.sha256(dat_bytes).hexdigest()
    if dat_digest != SOURCE_DAT_JSON_SHA256:
        raise SystemExit(
            f"unexpected Falcon DAT JSON SHA-256: {dat_digest}"
        )
    dat_data = json.loads(dat_bytes)
    source_dat = args.source_dat.read_bytes()
    source_dat_digest = hashlib.sha256(source_dat).hexdigest()
    if source_dat_digest != SOURCE_DAT_SHA256:
        raise SystemExit(
            f"unexpected PlCa.dat SHA-256: {source_dat_digest}"
        )
    animation_dat = args.animation_dat.read_bytes()
    animation_dat_digest = hashlib.sha256(animation_dat).hexdigest()
    if animation_dat_digest != SOURCE_ANIMATION_DAT_SHA256:
        raise SystemExit(
            "unexpected PlCaAJ.dat SHA-256: "
            f"{animation_dat_digest}"
        )
    digest = canonical_sha256(data)
    if digest != EXPECTED_CANONICAL_SHA256:
        raise SystemExit(
            f"unexpected Falcon frame-data SHA-256: {digest}"
        )
    output = generate(data, dat_data, source_dat, animation_dat)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8", newline="\n")
    print(
        "ssbm-falcon-frame-data=pass "
        f"slots={len(MOVE_KEYS)} "
        f"subactions={sum(data[key] is not None for key in MOVE_KEYS)} "
        f"source_sha256={digest} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
