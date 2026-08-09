#!/usr/bin/env python3
"""Generate a C binding for a manifest-owned numeric SSBM trace domain."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
from typing import Any


BUTTONS = {
    "attack": "PF_INPUT_BUTTON_ATTACK",
    "strong_attack": "PF_INPUT_BUTTON_STRONG_ATTACK",
    "special": "PF_INPUT_BUTTON_SPECIAL",
    "jump": "PF_INPUT_BUTTON_JUMP",
    "taunt": "PF_INPUT_BUTTON_TAUNT",
}


def identifier(value: Any, field: str) -> str:
    if not isinstance(value, str) or re.fullmatch(
        r"[A-Za-z_][A-Za-z0-9_]*", value
    ) is None:
        raise ValueError(f"{field} must be a C identifier")
    return value


def digest(value: Any, field: str) -> str:
    if (
        not isinstance(value, str)
        or len(value) != 64
        or any(character not in "0123456789abcdef" for character in value)
    ):
        raise ValueError(f"{field} must be a lowercase SHA-256")
    return value


def axis(value: Any, field: str) -> int:
    if (
        not isinstance(value, int)
        or isinstance(value, bool)
        or not -32767 <= value <= 32767
    ):
        raise ValueError(f"{field} must be a signed controller axis")
    return value


def unsigned_16(value: Any, field: str) -> int:
    if (
        not isinstance(value, int)
        or isinstance(value, bool)
        or not 0 <= value <= 65535
    ):
        raise ValueError(f"{field} must be an unsigned 16-bit integer")
    return value


def buttons(value: Any, field: str) -> str:
    if value is None:
        return "UINT64_C(0)"
    if not isinstance(value, list) or any(name not in BUTTONS for name in value):
        raise ValueError(f"{field} contains an unsupported button")
    if len(set(value)) != len(value):
        raise ValueError(f"{field} contains duplicate buttons")
    return " | ".join(BUTTONS[name] for name in value) or "UINT64_C(0)"


def stick(
    value: Any,
    field: str,
    *,
    invert_y: bool,
) -> tuple[int, int]:
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError(f"{field} must contain two controller axes")
    x = axis(value[0], f"{field}[0]")
    y = axis(value[1], f"{field}[1]")
    return x, -y if invert_y else y


def generate(manifest: dict[str, Any]) -> str:
    if manifest.get("schema") != 1:
        raise ValueError("unsupported trace coverage schema")
    stored = manifest.get("stored_oracle")
    checkpoint_pack = manifest.get("checkpoint_pack")
    if not isinstance(stored, dict) or stored.get("kind") != "numeric-trace-v1":
        raise ValueError("stored_oracle.kind must be numeric-trace-v1")
    if not isinstance(checkpoint_pack, dict):
        raise ValueError("checkpoint_pack must be an object")
    capture_plan = checkpoint_pack.get("capture_plan")
    if not isinstance(capture_plan, dict):
        raise ValueError("checkpoint_pack.capture_plan must be an object")
    raw_cases = stored.get("cases")
    sample_key = "inputs"
    if raw_cases is None:
        # Backward-compatible route for the first numeric domain. New domains
        # keep their simulator input sequence under stored_oracle so this
        # generic generator is independent of the live-capture choreography.
        raw_cases = capture_plan.get("damage_response_cases")
        sample_key = "hitlag"
    if not isinstance(raw_cases, list) or not raw_cases:
        raise ValueError("stored trace cases must be a non-empty list")
    c_config = stored.get("c")
    if not isinstance(c_config, dict):
        raise ValueError("stored_oracle.c must be an object")
    symbol_prefix = identifier(
        c_config.get("symbol_prefix"),
        "stored_oracle.c.symbol_prefix",
    )
    macro_prefix = identifier(
        c_config.get("macro_prefix"),
        "stored_oracle.c.macro_prefix",
    )
    samples_per_case = stored.get("samples_per_case")
    if (
        not isinstance(samples_per_case, int)
        or isinstance(samples_per_case, bool)
        or not 1 <= samples_per_case <= 64
    ):
        raise ValueError("samples_per_case must be in [1, 64]")
    source_digest = digest(
        stored.get("source_trace_sha256"),
        "stored_oracle.source_trace_sha256",
    )
    production_digest = digest(
        stored.get("production_trace_sha256"),
        "stored_oracle.production_trace_sha256",
    )

    rows: list[str] = []
    case_rows: list[str] = []
    ids: set[str] = set()
    for case_index, case in enumerate(raw_cases):
        if not isinstance(case, dict):
            raise ValueError(f"damage_response_cases[{case_index}] is invalid")
        case_id = case.get("id")
        samples = case.get(sample_key)
        if samples is None and sample_key == "inputs":
            samples = [{} for _ in range(samples_per_case)]
        if (
            not isinstance(case_id, str)
            or not case_id
            or case_id in ids
            or not isinstance(samples, list)
            or len(samples) != samples_per_case
        ):
            raise ValueError(f"invalid trace case {case_id!r}")
        ids.add(case_id)
        input_symbol = f"{symbol_prefix}_{case_id}_inputs"
        rows.extend(
            [
                "static const pf_ssbm_stored_trace_input",
                f"{input_symbol}[] = {{",
            ]
        )
        for sample_index, sample in enumerate(samples):
            if not isinstance(sample, dict):
                raise ValueError(
                    f"{case_id}.{sample_key}[{sample_index}] must be an object"
                )
            main_x, main_y = stick(
                sample.get("main", [0, 0]),
                f"{case_id}.{sample_key}[{sample_index}].main",
                invert_y=True,
            )
            c_x, c_y = stick(
                sample.get("c_stick", [0, 0]),
                f"{case_id}.{sample_key}[{sample_index}].c_stick",
                invert_y=True,
            )
            button_bits = buttons(
                sample.get("buttons"),
                f"{case_id}.{sample_key}[{sample_index}].buttons",
            )
            left_trigger = unsigned_16(
                sample.get("left_trigger", 0),
                f"{case_id}.{sample_key}[{sample_index}].left_trigger",
            )
            right_trigger = unsigned_16(
                sample.get("right_trigger", 0),
                f"{case_id}.{sample_key}[{sample_index}].right_trigger",
            )
            advance_ticks = unsigned_16(
                sample.get("advance_ticks", 1),
                f"{case_id}.{sample_key}[{sample_index}].advance_ticks",
            )
            if advance_ticks == 0:
                raise ValueError(
                    f"{case_id}.{sample_key}[{sample_index}].advance_ticks must be positive"
                )
            rows.append(
                "    { "
                f"INT16_C({main_x}), INT16_C({main_y}), "
                f"INT16_C({c_x}), INT16_C({c_y}), "
                f"{button_bits}, UINT16_C({left_trigger}), "
                f"UINT16_C({right_trigger}), UINT16_C({advance_ticks})"
                " },"
            )
        rows.extend(["};", ""])
        case_rows.append(
            f'    {{ {json.dumps(case_id)}, {input_symbol} }},'
        )

    return "\n".join(
        [
            "/* Generated by tools/generate_ssbm_stored_trace_oracle.py. */",
            f"#ifndef {macro_prefix}_ORACLE_INC",
            f"#define {macro_prefix}_ORACLE_INC",
            "",
            *rows,
            "static const pf_ssbm_stored_trace_case",
            f"{symbol_prefix}_cases[] = {{",
            *case_rows,
            "};",
            "",
            f"#define {macro_prefix}_CASE_COUNT UINT16_C({len(case_rows)})",
            f"#define {macro_prefix}_SAMPLES_PER_CASE "
            f"UINT8_C({samples_per_case})",
            f"#define {macro_prefix}_SOURCE_TRACE_SHA256 \\",
            f"    {json.dumps(source_digest)}",
            f"#define {macro_prefix}_PRODUCTION_TRACE_SHA256 \\",
            f"    {json.dumps(production_digest)}",
            "",
            "_Static_assert(",
            f"    sizeof({symbol_prefix}_cases) /",
            f"            sizeof({symbol_prefix}_cases[0]) ==",
            f"        {macro_prefix}_CASE_COUNT,",
            '    "stored trace case manifest is incomplete");',
            "",
            f"#endif /* {macro_prefix}_ORACLE_INC */",
            "",
        ]
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    generated = generate(manifest)
    if args.check:
        if not args.output.is_file() or args.output.read_text(
            encoding="utf-8"
        ) != generated:
            raise SystemExit(f"stored trace oracle is stale: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(generated, encoding="utf-8", newline="\n")
    print(
        "ssbm-stored-trace-generation=pass "
        f"domain={manifest['domain']} output={args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
