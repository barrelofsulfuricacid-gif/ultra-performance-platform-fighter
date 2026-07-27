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
| `buttons` | `(4, 2)` / `int8` | Per-player `[jump, forfeit]` bits |
| `main_stick` | `(4, 2)` / `int16` | Per-player x/y in `[-32768, 32767]` |
| `secondary_stick` | `(4, 2)` / `int16` | Per-player x/y in `[-32768, 32767]` |
| `triggers` | `(4, 2)` / `uint16` | Per-player left/right in `[0, 65535]` |

The observation is the exact 36-element `int32` compact layout documented in
`docs/technology_decisions/0008-rl-contract-candidate.md`. Gymnasium's scalar
reward uses the configured `reward_player` (player 0 by default);
`info["player_rewards_q16"]` retains exact rewards for all four slots.
