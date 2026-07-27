"""Gymnasium 1.3 vector environment backed by one batched C ABI call."""

from __future__ import annotations

from collections.abc import Sequence
from os import PathLike
from typing import Any

import gymnasium as gym
import numpy as np
from gymnasium.vector import AutoresetMode, VectorEnv
from gymnasium.vector.utils import batch_space

from ._native import (
    NativeBatch,
    NativeCallError,
    PF_Q16_ONE,
    PF_RL_COMPACT_VALUE_COUNT,
    PF_SIM_MAX_PLAYERS,
)

_UINT64_MASK = (1 << 64) - 1


class PlatformFighterError(RuntimeError):
    """The native simulation rejected a wrapper operation."""


class PlatformFighterVectorEnv(VectorEnv):
    """Vectorized duel/team environments using the platform-fighter C kernel.

    Each vector lane is one match. Actions control all fixed player slots.
    Gymnasium's scalar reward is selected from ``reward_player``; exact
    per-player Q16.16 and floating rewards remain available in ``info``.
    """

    metadata = {
        "render_modes": [],
        "render_fps": 60,
        "autoreset_mode": AutoresetMode.NEXT_STEP,
    }
    render_mode = None

    def __init__(
        self,
        num_envs: int,
        *,
        library_path: str | PathLike[str] | None = None,
        player_count: int = 2,
        max_ticks: int = 3_600,
        reward_player: int = 0,
    ):
        super().__init__()
        if num_envs <= 0:
            raise ValueError("num_envs must be positive")
        if player_count not in (2, 4):
            raise ValueError("player_count must be 2 (duel) or 4 (teams)")
        if reward_player < 0 or reward_player >= player_count:
            raise ValueError("reward_player must select an active player")

        self.num_envs = int(num_envs)
        self.player_count = int(player_count)
        self.reward_player = int(reward_player)
        self.autoreset_mode = AutoresetMode.NEXT_STEP
        self.metadata = dict(type(self).metadata)
        self._native = NativeBatch(
            library_path,
            self.num_envs,
            self.player_count,
            int(max_ticks),
        )

        self.single_observation_space = gym.spaces.Box(
            low=np.iinfo(np.int32).min,
            high=np.iinfo(np.int32).max,
            shape=(PF_RL_COMPACT_VALUE_COUNT,),
            dtype=np.int32,
        )
        self.observation_space = batch_space(
            self.single_observation_space, self.num_envs
        )
        self.single_action_space = gym.spaces.Dict(
            {
                "buttons": gym.spaces.MultiBinary(
                    (PF_SIM_MAX_PLAYERS, 2)
                ),
                "main_stick": gym.spaces.Box(
                    low=np.iinfo(np.int16).min,
                    high=np.iinfo(np.int16).max,
                    shape=(PF_SIM_MAX_PLAYERS, 2),
                    dtype=np.int16,
                ),
                "secondary_stick": gym.spaces.Box(
                    low=np.iinfo(np.int16).min,
                    high=np.iinfo(np.int16).max,
                    shape=(PF_SIM_MAX_PLAYERS, 2),
                    dtype=np.int16,
                ),
                "triggers": gym.spaces.Box(
                    low=0,
                    high=np.iinfo(np.uint16).max,
                    shape=(PF_SIM_MAX_PLAYERS, 2),
                    dtype=np.uint16,
                ),
            }
        )
        self.action_space = batch_space(
            self.single_action_space, self.num_envs
        )

        self._observations = np.zeros(
            (self.num_envs, PF_RL_COMPACT_VALUE_COUNT), dtype=np.int32
        )
        self._rewards = np.zeros(self.num_envs, dtype=np.float64)
        self._terminations = np.zeros(self.num_envs, dtype=np.bool_)
        self._truncations = np.zeros(self.num_envs, dtype=np.bool_)
        self._autoreset_envs = np.zeros(self.num_envs, dtype=np.bool_)
        self._episode_seeds = np.zeros(self.num_envs, dtype=np.uint64)
        self._has_reset = False

    def _normalize_reset_seeds(
        self, seed: int | Sequence[int | None] | None
    ) -> np.ndarray:
        if isinstance(seed, (int, np.integer)):
            seed_value = int(seed)
            if seed_value < 0 or seed_value > _UINT64_MASK:
                raise ValueError("seed must fit uint64")
            return np.asarray(
                [
                    (seed_value + environment_index) & _UINT64_MASK
                    for environment_index in range(self.num_envs)
                ],
                dtype=np.uint64,
            )

        if seed is not None:
            if len(seed) != self.num_envs:
                raise ValueError(
                    "seed sequence length must equal num_envs"
                )
            values: list[int] = []
            for item in seed:
                if item is None:
                    values.append(int(self.np_random.bit_generator.random_raw()))
                else:
                    value = int(item)
                    if value < 0 or value > _UINT64_MASK:
                        raise ValueError("every seed must fit uint64")
                    values.append(value)
            return np.asarray(values, dtype=np.uint64)

        return np.asarray(
            [
                int(self.np_random.bit_generator.random_raw())
                for _ in range(self.num_envs)
            ],
            dtype=np.uint64,
        )

    def _copy_transition(self, environment_index: int, transition: Any) -> None:
        self._observations[environment_index, :] = np.ctypeslib.as_array(
            transition.compact_observation.values
        )
        self._rewards[environment_index] = (
            int(transition.reward_q16[self.reward_player]) / PF_Q16_ONE
        )
        self._terminations[environment_index] = bool(
            transition.tick_result.terminated
        )
        self._truncations[environment_index] = bool(
            transition.tick_result.truncated
        )

    def _build_infos(
        self,
        indices: Sequence[int],
        transitions: Sequence[Any],
        autoreset: np.ndarray | None = None,
    ) -> dict[str, np.ndarray]:
        infos: dict[str, np.ndarray] = {
            "status": np.zeros(self.num_envs, dtype=np.uint32),
            "diagnostic_flags": np.zeros(self.num_envs, dtype=np.uint32),
            "winner_mask": np.zeros(self.num_envs, dtype=np.uint8),
            "player_rewards_q16": np.zeros(
                (self.num_envs, PF_SIM_MAX_PLAYERS), dtype=np.int32
            ),
            "player_rewards": np.zeros(
                (self.num_envs, PF_SIM_MAX_PLAYERS), dtype=np.float32
            ),
            "legal_buttons": np.zeros(
                (self.num_envs, PF_SIM_MAX_PLAYERS), dtype=np.uint64
            ),
            "_status": np.zeros(self.num_envs, dtype=np.bool_),
            "_diagnostic_flags": np.zeros(self.num_envs, dtype=np.bool_),
            "_winner_mask": np.zeros(self.num_envs, dtype=np.bool_),
            "_player_rewards_q16": np.zeros(
                self.num_envs, dtype=np.bool_
            ),
            "_player_rewards": np.zeros(self.num_envs, dtype=np.bool_),
            "_legal_buttons": np.zeros(self.num_envs, dtype=np.bool_),
        }
        for environment_index, transition in zip(
            indices, transitions, strict=True
        ):
            infos["status"][environment_index] = transition.status
            infos["diagnostic_flags"][environment_index] = (
                transition.diagnostic_flags
            )
            infos["winner_mask"][environment_index] = (
                transition.tick_result.winner_mask
            )
            rewards_q16 = np.ctypeslib.as_array(
                transition.reward_q16
            ).astype(np.int32, copy=True)
            infos["player_rewards_q16"][environment_index, :] = rewards_q16
            infos["player_rewards"][environment_index, :] = (
                rewards_q16.astype(np.float32) / PF_Q16_ONE
            )
            infos["legal_buttons"][environment_index, :] = (
                np.ctypeslib.as_array(transition.legal_buttons)
            )
            for key in (
                "_status",
                "_diagnostic_flags",
                "_winner_mask",
                "_player_rewards_q16",
                "_player_rewards",
                "_legal_buttons",
            ):
                infos[key][environment_index] = True

        if autoreset is not None:
            infos["autoreset"] = autoreset.copy()
            infos["_autoreset"] = np.ones(
                self.num_envs, dtype=np.bool_
            )
        return infos

    def reset(
        self,
        *,
        seed: int | Sequence[int | None] | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[np.ndarray, dict[str, np.ndarray]]:
        if self.closed:
            raise RuntimeError("cannot reset a closed environment")
        super().reset(
            seed=int(seed)
            if isinstance(seed, (int, np.integer))
            else None
        )
        seeds = self._normalize_reset_seeds(seed)

        reset_mask = np.ones(self.num_envs, dtype=np.bool_)
        if options is not None:
            unsupported = set(options) - {"reset_mask"}
            if unsupported:
                raise ValueError(
                    f"unsupported reset options: {sorted(unsupported)}"
                )
            if "reset_mask" in options:
                reset_mask = np.asarray(options["reset_mask"])
                if (
                    reset_mask.shape != (self.num_envs,)
                    or reset_mask.dtype != np.bool_
                    or not np.any(reset_mask)
                ):
                    raise ValueError(
                        "options['reset_mask'] must be a nonempty bool "
                        "array with shape (num_envs,)"
                    )
        if not self._has_reset and not np.all(reset_mask):
            raise RuntimeError(
                "the first reset must initialize every environment"
            )

        indices = np.flatnonzero(reset_mask).tolist()
        selected_seeds = [int(seeds[index]) for index in indices]
        try:
            transitions = self._native.reset(indices, selected_seeds)
        except NativeCallError as error:
            raise PlatformFighterError(str(error)) from error

        for environment_index, transition in zip(
            indices, transitions, strict=True
        ):
            self._copy_transition(environment_index, transition)
            self._episode_seeds[environment_index] = seeds[environment_index]
        self._rewards[reset_mask] = 0.0
        self._terminations[reset_mask] = False
        self._truncations[reset_mask] = False
        self._autoreset_envs[reset_mask] = False
        self._has_reset = True

        return self._observations.copy(), self._build_infos(
            indices, transitions
        )

    def _pack_actions(self, actions: dict[str, np.ndarray]) -> None:
        if not self.action_space.contains(actions):
            raise ValueError("actions are outside the declared action_space")

        buttons = np.asarray(actions["buttons"], dtype=np.uint8)
        main_stick = np.asarray(actions["main_stick"], dtype=np.int16)
        secondary_stick = np.asarray(
            actions["secondary_stick"], dtype=np.int16
        )
        triggers = np.asarray(actions["triggers"], dtype=np.uint16)

        action_view = self._native.action_view
        action_view["buttons"][:, :] = (
            buttons[:, :, 0].astype(np.uint64)
            | (buttons[:, :, 1].astype(np.uint64) << np.uint64(63))
        )
        action_view["main_stick_x"][:, :] = main_stick[:, :, 0]
        action_view["main_stick_y"][:, :] = main_stick[:, :, 1]
        action_view["secondary_stick_x"][:, :] = secondary_stick[:, :, 0]
        action_view["secondary_stick_y"][:, :] = secondary_stick[:, :, 1]
        action_view["left_trigger"][:, :] = triggers[:, :, 0]
        action_view["right_trigger"][:, :] = triggers[:, :, 1]

    def step(
        self, actions: dict[str, np.ndarray]
    ) -> tuple[
        np.ndarray,
        np.ndarray,
        np.ndarray,
        np.ndarray,
        dict[str, np.ndarray],
    ]:
        if self.closed:
            raise RuntimeError("cannot step a closed environment")
        if not self._has_reset:
            raise RuntimeError("reset must be called before step")
        self._pack_actions(actions)

        reset_indices = np.flatnonzero(self._autoreset_envs).tolist()
        active_indices = np.flatnonzero(~self._autoreset_envs).tolist()
        reset_transitions: list[Any] = []
        active_transitions: list[Any] = []
        try:
            if reset_indices:
                reset_seeds = []
                for environment_index in reset_indices:
                    next_seed = (
                        int(self._episode_seeds[environment_index])
                        + self.num_envs
                    ) & _UINT64_MASK
                    self._episode_seeds[environment_index] = next_seed
                    reset_seeds.append(next_seed)
                reset_transitions = self._native.reset(
                    reset_indices, reset_seeds
                )
            if active_indices:
                active_transitions = self._native.step(active_indices)
        except NativeCallError as error:
            raise PlatformFighterError(str(error)) from error

        self._rewards.fill(0.0)
        self._terminations.fill(False)
        self._truncations.fill(False)
        for environment_index, transition in zip(
            reset_indices, reset_transitions, strict=True
        ):
            self._copy_transition(environment_index, transition)
        for environment_index, transition in zip(
            active_indices, active_transitions, strict=True
        ):
            self._copy_transition(environment_index, transition)

        autoreset = np.zeros(self.num_envs, dtype=np.bool_)
        autoreset[reset_indices] = True
        all_indices = [*reset_indices, *active_indices]
        all_transitions = [*reset_transitions, *active_transitions]
        infos = self._build_infos(
            all_indices, all_transitions, autoreset=autoreset
        )
        self._autoreset_envs = np.logical_or(
            self._terminations, self._truncations
        )
        return (
            self._observations.copy(),
            self._rewards.copy(),
            self._terminations.copy(),
            self._truncations.copy(),
            infos,
        )

    def render(self) -> None:
        return None

    def close_extras(self, **kwargs: Any) -> None:
        del kwargs
        try:
            self._native.close()
        except NativeCallError as error:
            raise PlatformFighterError(str(error)) from error
