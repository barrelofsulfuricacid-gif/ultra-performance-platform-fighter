"""Private ctypes binding for the versioned platform-fighter RL C ABI."""

from __future__ import annotations

import ctypes as ct
import os
import sys
from pathlib import Path
from typing import Iterable

import numpy as np

PF_STATUS_OK = 0
PF_SIM_ABI_VERSION = 5
PF_SIM_CONTENT_SCHEMA_VERSION = 1
PF_RL_SCHEMA_VERSION = 14
PF_RL_ACTION_SCHEMA_VERSION = 1
PF_RL_TRANSITION_SCHEMA_VERSION = 12
PF_RL_COMPACT_OBSERVATION_SCHEMA_VERSION = 13
PF_SIM_MAX_PLAYERS = 4
PF_SIM_MAX_EVENTS_PER_TICK = 16
PF_RL_COMPACT_VALUE_COUNT = 102
PF_RL_BUTTON_BITS = (0, 1, 2, 3, 4, 63)
PF_RL_BUTTON_COUNT = len(PF_RL_BUTTON_BITS)
PF_INPUT_KNOWN_BUTTONS = sum(1 << bit for bit in PF_RL_BUTTON_BITS)
PF_F32_ONE = 65_536
PF_RL_REWARD_COMPONENT_TERMINAL = 1 << 0
PF_RL_REWARD_COMPONENT_ENGAGEMENT = 1 << 1
PF_RL_ENGAGEMENT_POTENTIAL_LIMIT_F32 = 16_384
PF_RL_ENGAGEMENT_REFERENCE_DISTANCE_F32 = 8_388_608


class NativeCallError(RuntimeError):
    """A C ABI operation returned a non-success status."""

    def __init__(self, operation: str, status: int, status_name: str):
        self.operation = operation
        self.status = status
        self.status_name = status_name
        super().__init__(f"{operation} failed: {status_name} ({status})")


class _Hash256(ct.Structure):
    _fields_ = [("bytes", ct.c_uint8 * 32)]


class _ContentView(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("schema_version", ct.c_uint16),
        ("reserved", ct.c_uint16),
        ("bytes", ct.c_void_p),
        ("byte_count", ct.c_size_t),
        ("content_hash", _Hash256),
    ]


class _SimConfig(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("schema_version", ct.c_uint16),
        ("player_count", ct.c_uint8),
        ("mode", ct.c_uint8),
        ("max_ticks", ct.c_uint64),
        ("arena_half_width_f32", ct.c_int32),
        ("arena_ceiling_f32", ct.c_int32),
        ("stock_count", ct.c_uint8),
        ("reserved2", ct.c_uint8),
        ("respawn_delay_ticks", ct.c_uint16),
        ("respawn_invulnerability_ticks", ct.c_uint16),
        ("reserved3", ct.c_uint16),
    ]


class _MemoryRequirements(ct.Structure):
    _fields_ = [
        ("state_bytes", ct.c_size_t),
        ("state_alignment", ct.c_size_t),
        ("scratch_bytes", ct.c_size_t),
        ("scratch_alignment", ct.c_size_t),
    ]


class _SimEvent(ct.Structure):
    _fields_ = [
        ("tick", ct.c_uint64),
        ("sequence", ct.c_uint32),
        ("value_f32", ct.c_uint32),
        ("velocity_x_f32", ct.c_int32),
        ("velocity_y_f32", ct.c_int32),
        ("type", ct.c_uint16),
        ("flags", ct.c_uint16),
        ("detail", ct.c_uint16),
        ("source_player", ct.c_uint8),
        ("target_player", ct.c_uint8),
    ]


class _TickResult(ct.Structure):
    _fields_ = [
        ("completed_tick", ct.c_uint64),
        ("fault_flags", ct.c_uint32),
        ("terminated", ct.c_uint8),
        ("truncated", ct.c_uint8),
        ("winner_mask", ct.c_uint8),
        ("reserved", ct.c_uint8),
        ("event_count", ct.c_uint8),
        ("reserved2", ct.c_uint8),
        ("reserved3", ct.c_uint16),
        ("events", _SimEvent * PF_SIM_MAX_EVENTS_PER_TICK),
    ]


class _PlayerObservation(ct.Structure):
    _fields_ = [
        ("previous_buttons", ct.c_uint64),
        ("position_x_f32", ct.c_int32),
        ("position_y_f32", ct.c_int32),
        ("velocity_x_f32", ct.c_int32),
        ("velocity_y_f32", ct.c_int32),
        ("player_slot", ct.c_uint8),
        ("team", ct.c_uint8),
        ("grounded", ct.c_uint8),
        ("active", ct.c_uint8),
        ("stocks_remaining", ct.c_uint8),
        ("recovery_available", ct.c_uint8),
        ("respawn_ticks", ct.c_uint16),
        ("respawn_invulnerability_ticks", ct.c_uint16),
        ("charge_ticks", ct.c_uint16),
        ("smash_charge_ticks", ct.c_uint16),
        ("shield_strength", ct.c_uint16),
        ("shield_tilt_x", ct.c_int16),
        ("shield_tilt_y", ct.c_int16),
        ("shield_health_f32", ct.c_uint32),
        ("stale_move_multiplier_f32", ct.c_uint32),
        ("stale_move_count", ct.c_uint8),
        ("stale_move_ids", ct.c_uint8 * 9),
        ("prone_orientation", ct.c_uint8),
        ("reserved2", ct.c_uint8),
    ]


class _ItemObservation(ct.Structure):
    _fields_ = [
        ("position_x_f32", ct.c_int32),
        ("position_y_f32", ct.c_int32),
        ("velocity_x_f32", ct.c_int32),
        ("velocity_y_f32", ct.c_int32),
        ("lifetime_ticks", ct.c_uint16),
        ("respawn_ticks", ct.c_uint16),
        ("pickup_lockout_ticks", ct.c_uint16),
        ("state", ct.c_uint8),
        ("holder_slot", ct.c_uint8),
        ("source_slot", ct.c_uint8),
        ("throw_direction", ct.c_uint8),
        ("hit_mask", ct.c_uint8),
        ("reserved", ct.c_uint8 * 3),
    ]


class _ProjectileObservation(ct.Structure):
    _fields_ = [
        ("position_x_f32", ct.c_int32),
        ("position_y_f32", ct.c_int32),
        ("velocity_x_f32", ct.c_int32),
        ("velocity_y_f32", ct.c_int32),
        ("lifetime_ticks", ct.c_uint16),
        ("state", ct.c_uint8),
        ("owner_slot", ct.c_uint8),
    ]


class _SimObservation(ct.Structure):
    _fields_ = [
        ("tick", ct.c_uint64),
        ("seed", ct.c_uint64),
        ("fault_flags", ct.c_uint32),
        ("schema_version", ct.c_uint16),
        ("player_count", ct.c_uint8),
        ("mode", ct.c_uint8),
        ("terminated", ct.c_uint8),
        ("truncated", ct.c_uint8),
        ("winner_mask", ct.c_uint8),
        ("sudden_death", ct.c_uint8),
        ("stock_count", ct.c_uint8),
        ("reserved", ct.c_uint8),
        ("item", _ItemObservation),
        ("projectile", _ProjectileObservation),
        ("players", _PlayerObservation * PF_SIM_MAX_PLAYERS),
    ]


class _RlCompactObservation(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("schema_version", ct.c_uint16),
        ("value_count", ct.c_uint16),
        ("values", ct.c_int32 * PF_RL_COMPACT_VALUE_COUNT),
    ]


class _RlAction(ct.Structure):
    _fields_ = [
        ("buttons", ct.c_uint64),
        ("main_stick_x", ct.c_int16),
        ("main_stick_y", ct.c_int16),
        ("secondary_stick_x", ct.c_int16),
        ("secondary_stick_y", ct.c_int16),
        ("left_trigger", ct.c_uint16),
        ("right_trigger", ct.c_uint16),
        ("schema_version", ct.c_uint16),
        ("reserved", ct.c_uint16),
    ]


class _RlSpec(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("schema_version", ct.c_uint16),
        ("action_schema_version", ct.c_uint16),
        ("transition_schema_version", ct.c_uint16),
        ("compact_observation_schema_version", ct.c_uint16),
        ("compact_value_count", ct.c_uint16),
        ("action_stride", ct.c_uint16),
        ("max_players", ct.c_uint8),
        ("reward_component_flags", ct.c_uint8),
        ("reserved", ct.c_uint8 * 2),
        ("known_buttons", ct.c_uint64),
        ("axis_minimum", ct.c_int16),
        ("axis_maximum", ct.c_int16),
        ("trigger_minimum", ct.c_uint16),
        ("trigger_maximum", ct.c_uint16),
        ("terminal_reward_one_f32", ct.c_int32),
        ("engagement_potential_limit_f32", ct.c_int32),
    ]


class _RlTransition(ct.Structure):
    _fields_ = [
        ("struct_size", ct.c_uint32),
        ("schema_version", ct.c_uint16),
        ("reserved", ct.c_uint16),
        ("status", ct.c_uint32),
        ("diagnostic_flags", ct.c_uint32),
        ("tick_result", _TickResult),
        ("structured_observation", _SimObservation),
        ("compact_observation", _RlCompactObservation),
        ("reward_f32", ct.c_int32 * PF_SIM_MAX_PLAYERS),
        ("legal_buttons", ct.c_uint64 * PF_SIM_MAX_PLAYERS),
    ]


_ACTION_DTYPE = np.dtype(
    {
        "names": [
            "buttons",
            "main_stick_x",
            "main_stick_y",
            "secondary_stick_x",
            "secondary_stick_y",
            "left_trigger",
            "right_trigger",
            "schema_version",
            "reserved",
        ],
        "formats": [
            np.uint64,
            np.int16,
            np.int16,
            np.int16,
            np.int16,
            np.uint16,
            np.uint16,
            np.uint16,
            np.uint16,
        ],
        "offsets": [0, 8, 10, 12, 14, 16, 18, 20, 22],
        "itemsize": 24,
    }
)


def _assert_layouts() -> None:
    expected = {
        "_ContentView": (_ContentView, 56),
        "_SimConfig": (_SimConfig, 32),
        "_MemoryRequirements": (_MemoryRequirements, 32),
        "_SimEvent": (_SimEvent, 32),
        "_TickResult": (_TickResult, 536),
        "_PlayerObservation": (_PlayerObservation, 64),
        "_ItemObservation": (_ItemObservation, 32),
        "_ProjectileObservation": (_ProjectileObservation, 20),
        "_SimObservation": (_SimObservation, 344),
        "_RlAction": (_RlAction, 24),
        "_RlSpec": (_RlSpec, 48),
        "_RlCompactObservation": (_RlCompactObservation, 416),
        "_RlTransition": (_RlTransition, 1360),
    }
    mismatches = [
        f"{name}={ct.sizeof(struct_type)} expected={size}"
        for name, (struct_type, size) in expected.items()
        if ct.sizeof(struct_type) != size
    ]
    if mismatches:
        raise RuntimeError(
            "unsupported native C layout: " + ", ".join(mismatches)
        )
    if _ACTION_DTYPE.itemsize != ct.sizeof(_RlAction):
        raise RuntimeError("NumPy and ctypes RL action layouts disagree")


def _library_names() -> tuple[str, ...]:
    if sys.platform == "win32":
        return ("pf_sim_rl.dll",)
    if sys.platform == "darwin":
        return ("libpf_sim_rl.dylib",)
    return ("libpf_sim_rl.so",)


def resolve_library_path(explicit_path: str | os.PathLike[str] | None) -> Path:
    """Resolve an explicit, environment, or local CMake shared-library path."""

    candidates: list[Path] = []
    if explicit_path is not None:
        candidates.append(Path(explicit_path))
    environment_path = os.environ.get("PF_SIM_LIBRARY")
    if environment_path:
        candidates.append(Path(environment_path))

    repository_root = Path(__file__).resolve().parents[4]
    for build_name in ("headless", "release", "debug", "profile"):
        for library_name in _library_names():
            candidates.append(repository_root / "build" / build_name / library_name)
            candidates.append(
                repository_root
                / "build"
                / build_name
                / "Release"
                / library_name
            )

    for candidate in candidates:
        resolved = candidate.expanduser().resolve()
        if resolved.is_file():
            return resolved

    searched = ", ".join(str(path) for path in candidates)
    raise FileNotFoundError(
        "platform-fighter RL shared library was not found; build the "
        f"headless preset or pass library_path=. Searched: {searched}"
    )


class _AlignedBuffer:
    def __init__(self, size: int, alignment: int):
        if size <= 0 or alignment <= 0 or alignment & (alignment - 1):
            raise ValueError("native memory size/alignment is invalid")
        self.raw = ct.create_string_buffer(size + alignment - 1)
        base = ct.addressof(self.raw)
        self.address = (base + alignment - 1) & ~(alignment - 1)
        self.pointer = ct.c_void_p(self.address)
        self.size = size


class NativeBatch:
    """Own native simulation buffers and issue single or batched ABI calls."""

    def __init__(
        self,
        library_path: str | os.PathLike[str] | None,
        environment_count: int,
        player_count: int,
        max_ticks: int,
    ):
        _assert_layouts()
        if environment_count <= 0:
            raise ValueError("environment_count must be positive")
        if player_count not in (2, 4):
            raise ValueError("player_count must be 2 (duel) or 4 (teams)")
        if not 0 < max_ticks < (1 << 64) - 1:
            raise ValueError("max_ticks must be in [1, 2^64-2]")

        self.environment_count = environment_count
        self.player_count = player_count
        self.library_path = resolve_library_path(library_path)
        self.lib = ct.CDLL(str(self.library_path))
        self._configure_functions()
        self._closed = False

        if int(self.lib.pf_sim_abi_version()) != PF_SIM_ABI_VERSION:
            raise RuntimeError("native simulation ABI version is not supported")

        self.spec = _RlSpec()
        self._check("pf_rl_query_spec", self.lib.pf_rl_query_spec(ct.byref(self.spec)))
        if (
            self.spec.struct_size != ct.sizeof(_RlSpec)
            or self.spec.schema_version != PF_RL_SCHEMA_VERSION
            or self.spec.action_schema_version != PF_RL_ACTION_SCHEMA_VERSION
            or self.spec.transition_schema_version
            != PF_RL_TRANSITION_SCHEMA_VERSION
            or self.spec.compact_observation_schema_version
            != PF_RL_COMPACT_OBSERVATION_SCHEMA_VERSION
            or self.spec.compact_value_count != PF_RL_COMPACT_VALUE_COUNT
            or self.spec.action_stride != PF_SIM_MAX_PLAYERS
            or self.spec.max_players != PF_SIM_MAX_PLAYERS
            or self.spec.known_buttons != PF_INPUT_KNOWN_BUTTONS
            or self.spec.axis_minimum != -(1 << 15)
            or self.spec.axis_maximum != (1 << 15) - 1
            or self.spec.trigger_minimum != 0
            or self.spec.trigger_maximum != (1 << 16) - 1
            or self.spec.reward_component_flags
            != (
                PF_RL_REWARD_COMPONENT_TERMINAL
                | PF_RL_REWARD_COMPONENT_ENGAGEMENT
            )
            or self.spec.terminal_reward_one_f32 != PF_F32_ONE
            or self.spec.engagement_potential_limit_f32
            != PF_RL_ENGAGEMENT_POTENTIAL_LIMIT_F32
        ):
            raise RuntimeError("native RL schema does not match the Python binding")

        config = _SimConfig()
        mode = 1 if player_count == 2 else 2
        self._check(
            "pf_sim_default_config",
            self.lib.pf_sim_default_config(
                ct.byref(config), player_count, mode
            ),
        )
        config.max_ticks = max_ticks

        requirements = _MemoryRequirements()
        self._check(
            "pf_sim_query_memory",
            self.lib.pf_sim_query_memory(
                ct.byref(config), ct.byref(requirements)
            ),
        )

        content = _ContentView()
        content.struct_size = ct.sizeof(_ContentView)
        content.schema_version = PF_SIM_CONTENT_SCHEMA_VERSION

        self._state_buffers: list[_AlignedBuffer] = []
        self._scratch_buffers: list[_AlignedBuffer] = []
        self.sims = (ct.c_void_p * environment_count)()
        for index in range(environment_count):
            state = _AlignedBuffer(
                int(requirements.state_bytes),
                int(requirements.state_alignment),
            )
            scratch = _AlignedBuffer(
                int(requirements.scratch_bytes),
                int(requirements.scratch_alignment),
            )
            sim = ct.c_void_p()
            self._check(
                f"pf_sim_init[{index}]",
                self.lib.pf_sim_init(
                    state.pointer,
                    state.size,
                    scratch.pointer,
                    scratch.size,
                    ct.byref(content),
                    ct.byref(config),
                    ct.byref(sim),
                ),
            )
            self._state_buffers.append(state)
            self._scratch_buffers.append(scratch)
            self.sims[index] = sim

        self.actions = (
            _RlAction * (environment_count * PF_SIM_MAX_PLAYERS)
        )()
        action_bytes = np.ctypeslib.as_array(
            (ct.c_uint8 * ct.sizeof(self.actions)).from_address(
                ct.addressof(self.actions)
            )
        )
        self.action_view = action_bytes.view(_ACTION_DTYPE).reshape(
            environment_count, PF_SIM_MAX_PLAYERS
        )
        self.action_view["schema_version"].fill(
            PF_RL_ACTION_SCHEMA_VERSION
        )
        self.transitions = (_RlTransition * environment_count)()

    def _configure_functions(self) -> None:
        self.lib.pf_sim_abi_version.argtypes = []
        self.lib.pf_sim_abi_version.restype = ct.c_uint32
        self.lib.pf_status_name.argtypes = [ct.c_int]
        self.lib.pf_status_name.restype = ct.c_char_p
        self.lib.pf_sim_default_config.argtypes = [
            ct.POINTER(_SimConfig),
            ct.c_uint8,
            ct.c_int,
        ]
        self.lib.pf_sim_default_config.restype = ct.c_int
        self.lib.pf_sim_query_memory.argtypes = [
            ct.POINTER(_SimConfig),
            ct.POINTER(_MemoryRequirements),
        ]
        self.lib.pf_sim_query_memory.restype = ct.c_int
        self.lib.pf_sim_init.argtypes = [
            ct.c_void_p,
            ct.c_size_t,
            ct.c_void_p,
            ct.c_size_t,
            ct.POINTER(_ContentView),
            ct.POINTER(_SimConfig),
            ct.POINTER(ct.c_void_p),
        ]
        self.lib.pf_sim_init.restype = ct.c_int
        self.lib.pf_sim_deinit.argtypes = [ct.c_void_p]
        self.lib.pf_sim_deinit.restype = ct.c_int
        self.lib.pf_rl_query_spec.argtypes = [ct.POINTER(_RlSpec)]
        self.lib.pf_rl_query_spec.restype = ct.c_int
        self.lib.pf_rl_reset_batch.argtypes = [
            ct.POINTER(ct.c_void_p),
            ct.POINTER(ct.c_uint64),
            ct.c_size_t,
            ct.POINTER(_RlTransition),
        ]
        self.lib.pf_rl_reset_batch.restype = ct.c_int
        self.lib.pf_rl_step.argtypes = [
            ct.c_void_p,
            ct.POINTER(_RlAction),
            ct.c_size_t,
            ct.POINTER(_RlTransition),
        ]
        self.lib.pf_rl_step.restype = ct.c_int
        self.lib.pf_rl_step_batch.argtypes = [
            ct.POINTER(ct.c_void_p),
            ct.c_size_t,
            ct.POINTER(_RlAction),
            ct.c_size_t,
            ct.POINTER(_RlTransition),
        ]
        self.lib.pf_rl_step_batch.restype = ct.c_int

    def _status_name(self, status: int) -> str:
        encoded = self.lib.pf_status_name(status)
        return encoded.decode("ascii") if encoded else "unknown-status"

    def _check(self, operation: str, status: int) -> None:
        if status != PF_STATUS_OK:
            raise NativeCallError(
                operation, int(status), self._status_name(int(status))
            )

    def _selection(
        self, indices: Iterable[int]
    ) -> tuple[list[int], ct.Array[ct.c_void_p]]:
        selected = [int(index) for index in indices]
        if not selected:
            return selected, (ct.c_void_p * 0)()
        if len(set(selected)) != len(selected) or any(
            index < 0 or index >= self.environment_count for index in selected
        ):
            raise ValueError("environment selection is invalid")
        sims = (ct.c_void_p * len(selected))(
            *(self.sims[index] for index in selected)
        )
        return selected, sims

    def reset(
        self, indices: Iterable[int], seeds: Iterable[int]
    ) -> list[_RlTransition]:
        selected, sims = self._selection(indices)
        seed_values = [int(seed) for seed in seeds]
        if len(seed_values) != len(selected):
            raise ValueError("reset seeds do not match environment selection")
        if any(seed < 0 or seed >= 1 << 64 for seed in seed_values):
            raise ValueError("reset seeds must fit uint64")
        if not selected:
            return []

        native_seeds = (ct.c_uint64 * len(selected))(*seed_values)
        transitions = (_RlTransition * len(selected))()
        status = self.lib.pf_rl_reset_batch(
            sims, native_seeds, len(selected), transitions
        )
        self._check_transitions(
            "pf_rl_reset_batch", status, selected, transitions
        )
        return list(transitions)

    def step(self, indices: Iterable[int]) -> list[_RlTransition]:
        selected, sims = self._selection(indices)
        if not selected:
            return []

        if selected == list(range(self.environment_count)):
            status = self.lib.pf_rl_step_batch(
                self.sims,
                self.environment_count,
                self.actions,
                PF_SIM_MAX_PLAYERS,
                self.transitions,
            )
            self._check_transitions(
                "pf_rl_step_batch", status, selected, self.transitions
            )
            return list(self.transitions)

        subset_actions = (
            _RlAction * (len(selected) * PF_SIM_MAX_PLAYERS)
        )()
        for destination, source in enumerate(selected):
            ct.memmove(
                ct.addressof(subset_actions)
                + destination * PF_SIM_MAX_PLAYERS * ct.sizeof(_RlAction),
                ct.addressof(self.actions)
                + source * PF_SIM_MAX_PLAYERS * ct.sizeof(_RlAction),
                PF_SIM_MAX_PLAYERS * ct.sizeof(_RlAction),
            )
        transitions = (_RlTransition * len(selected))()
        status = self.lib.pf_rl_step_batch(
            sims,
            len(selected),
            subset_actions,
            PF_SIM_MAX_PLAYERS,
            transitions,
        )
        self._check_transitions(
            "pf_rl_step_batch", status, selected, transitions
        )
        return list(transitions)

    def step_all_single(self) -> None:
        """Step every environment through one Python-to-C call per sim."""

        for environment_index in range(self.environment_count):
            action_offset = environment_index * PF_SIM_MAX_PLAYERS
            status = self.lib.pf_rl_step(
                self.sims[environment_index],
                ct.cast(
                    ct.byref(
                        self.actions,
                        action_offset * ct.sizeof(_RlAction),
                    ),
                    ct.POINTER(_RlAction),
                ),
                self.player_count,
                ct.byref(self.transitions[environment_index]),
            )
            self._check(
                f"pf_rl_step[{environment_index}]", int(status)
            )

    def step_all_batch(self) -> None:
        """Step every environment through one Python-to-C batch call."""

        status = self.lib.pf_rl_step_batch(
            self.sims,
            self.environment_count,
            self.actions,
            PF_SIM_MAX_PLAYERS,
            self.transitions,
        )
        self._check("pf_rl_step_batch", int(status))

    def _check_transitions(
        self,
        operation: str,
        status: int,
        selected: list[int],
        transitions: ct.Array[_RlTransition],
    ) -> None:
        if status == PF_STATUS_OK:
            return
        failures = [
            (selected[index], int(transition.status))
            for index, transition in enumerate(transitions)
            if transition.status != PF_STATUS_OK
        ]
        detail = ", ".join(
            f"env {index}: {self._status_name(item_status)} ({item_status})"
            for index, item_status in failures
        )
        raise NativeCallError(
            f"{operation} [{detail}]",
            int(status),
            self._status_name(int(status)),
        )

    def close(self) -> None:
        if self._closed:
            return
        first_error: NativeCallError | None = None
        for index in range(self.environment_count):
            status = int(self.lib.pf_sim_deinit(self.sims[index]))
            if status != PF_STATUS_OK and first_error is None:
                first_error = NativeCallError(
                    f"pf_sim_deinit[{index}]",
                    status,
                    self._status_name(status),
                )
            self.sims[index] = None
        self._closed = True
        if first_error is not None:
            raise first_error


__all__ = [
    "NativeBatch",
    "NativeCallError",
    "PF_F32_ONE",
    "PF_RL_BUTTON_BITS",
    "PF_RL_BUTTON_COUNT",
    "PF_RL_COMPACT_VALUE_COUNT",
    "PF_SIM_MAX_PLAYERS",
]
