# Platform Fighter Gymnasium wrapper

This package is a thin `gymnasium.vector.VectorEnv` adapter over the
authoritative C RL ABI in `include/pf/rl.h`. It owns native state/scratch
buffers and NumPy conversion only; reset, stepping, rewards, termination,
truncation, hashing, and replay semantics remain in C.

Build the native shared library first:

```sh
./tools/workflow.sh headless
```

Then install the pinned wrapper dependency and run the API tests:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -e bindings/python
PF_SIM_LIBRARY=build/headless/libpf_sim_rl.so \
    .venv/bin/python -m unittest discover \
    -s bindings/python/tests -v
```

On macOS the library is `libpf_sim_rl.dylib`; on Windows it is
`pf_sim_rl.dll`. Supplying `library_path=` to `PlatformFighterVectorEnv`
overrides `PF_SIM_LIBRARY`.

Each vector lane is one complete match. The single-lane action is a
dictionary:

| Key | Shape / dtype | Meaning |
|---|---|---|
| `buttons` | `(4, 6)` / `int8` | Per-player `[jump, attack, strong, special, taunt, forfeit]` bits |
| `main_stick` | `(4, 2)` / `int16` | Per-player x/y in `[-32768, 32767]` |
| `secondary_stick` | `(4, 2)` / `int16` | Per-player x/y in `[-32768, 32767]` |
| `triggers` | `(4, 2)` / `uint16` | Per-player left/right in `[0, 65535]` |

The observation is the exact 102-element `int32` compact schema-13 layout in
`include/pf/rl.h`. Words 2–3 are reserved zero, so the reset seed is not
exposed to the policy. The layout contains global match state, four fixed
player records, the item and projectile slots, per-player charge, smash,
shield strength/health/tilt, and packed stale-move records. Existing indices
remain stable across the append-only compact-schema migrations.

Gymnasium's scalar reward uses the configured `reward_player` (player 0 by
default) and combines the bounded engagement-potential delta with the terminal
match outcome. `info["player_rewards_q16"]` retains exact rewards for all four
slots, and `info["legal_buttons"]` retains the native 64-bit legal-input masks.
