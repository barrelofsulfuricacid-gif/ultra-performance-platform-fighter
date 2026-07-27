#!/usr/bin/env python3
"""Measure Python-to-C call overhead for single and batched RL stepping."""

from __future__ import annotations

import argparse
import statistics
import time

from pf_gymnasium._native import NativeBatch


def _measure(batch: NativeBatch, rounds: int, batched: bool) -> float:
    started = time.perf_counter()
    for _ in range(rounds):
        if batched:
            batch.step_all_batch()
        else:
            batch.step_all_single()
    return time.perf_counter() - started


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True)
    parser.add_argument("--environments", type=int, default=64)
    parser.add_argument("--rounds", type=int, default=2_048)
    parser.add_argument("--samples", type=int, default=5)
    args = parser.parse_args()
    if args.environments <= 0 or args.rounds <= 0 or args.samples <= 0:
        parser.error("environments, rounds, and samples must be positive")

    single_samples: list[float] = []
    batch_samples: list[float] = []
    native = NativeBatch(
        args.library,
        args.environments,
        player_count=2,
        max_ticks=args.rounds + 1,
    )
    try:
        indices = range(args.environments)
        seeds = range(10_000, 10_000 + args.environments)
        for _ in range(args.samples):
            native.reset(indices, seeds)
            single_samples.append(_measure(native, args.rounds, False))
            native.reset(indices, seeds)
            batch_samples.append(_measure(native, args.rounds, True))
    finally:
        native.close()

    single_seconds = statistics.median(single_samples)
    batch_seconds = statistics.median(batch_samples)
    total_ticks = args.environments * args.rounds
    speedup = single_seconds / batch_seconds
    outcome = "pass" if speedup > 1.0 else "fail"
    print(
        f"python-boundary={outcome} environments={args.environments} "
        f"rounds={args.rounds} "
        f"single_tps={total_ticks / single_seconds:.0f} "
        f"batch_tps={total_ticks / batch_seconds:.0f} "
        f"boundary_speedup={speedup:.4f}"
    )
    return 0 if outcome == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
