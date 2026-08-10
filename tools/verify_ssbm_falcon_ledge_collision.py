#!/usr/bin/env python3
"""Qualify Falcon's ledge-option live hit/miss collision discriminator."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from verify_ssbm_falcon_common_hurt import (
    sha256,
    verify_ledge_collision_discriminator,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("coverage_manifest", type=Path)
    parser.add_argument("hurt_profile", type=Path)
    parser.add_argument("import_manifest", type=Path)
    args = parser.parse_args()

    profile = json.loads(args.hurt_profile.read_text(encoding="utf-8"))
    import_manifest = json.loads(args.import_manifest.read_text(encoding="utf-8"))
    if (
        import_manifest.get("schema") != 1
        or import_manifest.get("scope") != "ssbm-hurt-pose-import"
        or import_manifest.get("fighter") != "CPTFALCON"
        or sha256(args.hurt_profile) != import_manifest.get("profile_sha256")
        or profile.get("capture_sha256") != import_manifest.get("capture_sha256")
        or profile.get("semantic_sha256") != import_manifest.get("semantic_sha256")
    ):
        raise SystemExit("invalid Falcon ledge hurt import manifest")
    qualification = import_manifest.get("live_collision_qualification")
    if not isinstance(qualification, dict):
        raise SystemExit("Falcon ledge live collision qualification is missing")
    semantic_sha256, hit_margin, miss_margin, generic_margin = (
        verify_ledge_collision_discriminator(
            args.capture,
            args.coverage_manifest,
            profile,
            qualification,
        )
    )
    print(
        "ssbm-falcon-ledge-collision=pass "
        f"semantic_sha256={semantic_sha256} "
        f"hit_margin={hit_margin:.9f} "
        f"miss_margin={miss_margin:.9f} "
        f"generic_margin={generic_margin:.9f}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
