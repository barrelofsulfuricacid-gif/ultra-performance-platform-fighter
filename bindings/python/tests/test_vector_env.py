"""API and determinism tests for the Gymnasium vector adapter."""

from __future__ import annotations

import os
import unittest

import gymnasium
import numpy as np

from pf_gymnasium import PlatformFighterVectorEnv


def _zero_actions(environment: PlatformFighterVectorEnv) -> dict[str, np.ndarray]:
    return {
        "buttons": np.zeros(
            (environment.num_envs, 4, 6), dtype=np.int8
        ),
        "main_stick": np.zeros(
            (environment.num_envs, 4, 2), dtype=np.int16
        ),
        "secondary_stick": np.zeros(
            (environment.num_envs, 4, 2), dtype=np.int16
        ),
        "triggers": np.zeros(
            (environment.num_envs, 4, 2), dtype=np.uint16
        ),
    }


class PlatformFighterVectorEnvTests(unittest.TestCase):
    def setUp(self) -> None:
        self.assertEqual(gymnasium.__version__, "1.3.0")
        library_path = os.environ.get("PF_SIM_LIBRARY")
        if not library_path:
            self.fail("PF_SIM_LIBRARY must identify the built shared library")
        self.library_path = library_path

    def test_spaces_reset_and_determinism(self) -> None:
        first = PlatformFighterVectorEnv(
            6, library_path=self.library_path, player_count=2
        )
        second = PlatformFighterVectorEnv(
            6, library_path=self.library_path, player_count=2
        )
        self.addCleanup(first.close)
        self.addCleanup(second.close)

        first_observation, first_info = first.reset(seed=1234)
        second_observation, second_info = second.reset(seed=1234)
        self.assertTrue(first.observation_space.contains(first_observation))
        self.assertTrue(
            np.array_equal(first_observation, second_observation)
        )
        self.assertEqual(first_observation.shape, (6, 102))
        self.assertEqual(first_observation.dtype, np.int32)
        self.assertTrue(np.all(first_observation[:, 2:4] == 0))
        self.assertTrue(
            np.all((first_observation[:, 7] >> 19) & 0x7F == 4)
        )
        self.assertTrue(np.all(first_observation[:, [15, 25]] == 4))
        self.assertTrue(np.all(first_observation[:, [16, 17, 26, 27]] == 0))
        self.assertTrue(np.all(first_observation[:, 62:74] == 0))
        self.assertTrue(
            np.all(first_observation[:, [86, 90, 94, 98]] == 65536)
        )
        self.assertTrue(np.all(first_info["_legal_buttons"]))
        self.assertTrue(
            np.array_equal(
                first_info["legal_buttons"],
                second_info["legal_buttons"],
            )
        )

        actions = _zero_actions(first)
        actions["main_stick"][:, 0, 0] = 32767
        actions["main_stick"][:, 1, 0] = -32768
        self.assertTrue(first.action_space.contains(actions))
        first_step = first.step(actions)
        second_step = second.step(actions)
        for first_value, second_value in zip(
            first_step[:4], second_step[:4], strict=True
        ):
            self.assertTrue(np.array_equal(first_value, second_value))
        self.assertTrue(
            np.array_equal(
                first_step[4]["player_rewards_q16"],
                second_step[4]["player_rewards_q16"],
            )
        )
        self.assertTrue(np.all(first_step[1] > 0.0))

    def test_duel_reward_and_next_step_autoreset(self) -> None:
        environment = PlatformFighterVectorEnv(
            3, library_path=self.library_path, player_count=2
        )
        self.addCleanup(environment.close)
        environment.reset(seed=99)

        actions = _zero_actions(environment)
        actions["buttons"][0, 0, 5] = 1
        _, rewards, terminated, truncated, info = environment.step(actions)
        self.assertTrue(terminated[0])
        self.assertFalse(truncated[0])
        self.assertEqual(rewards[0], -1.0)
        np.testing.assert_array_equal(
            info["player_rewards_q16"][0],
            np.asarray([-65536, 65536, 0, 0], dtype=np.int32),
        )
        self.assertFalse(np.any(info["legal_buttons"][0]))

        observation, rewards, terminated, truncated, info = (
            environment.step(_zero_actions(environment))
        )
        self.assertTrue(info["autoreset"][0])
        self.assertEqual(rewards[0], 0.0)
        self.assertFalse(terminated[0])
        self.assertFalse(truncated[0])
        self.assertEqual(int(observation[0, 0]), 0)
        self.assertTrue(np.any(info["legal_buttons"][0]))

    def test_team_reward_player_selection(self) -> None:
        environment = PlatformFighterVectorEnv(
            2,
            library_path=self.library_path,
            player_count=4,
            reward_player=2,
        )
        self.addCleanup(environment.close)
        environment.reset(seed=7)
        actions = _zero_actions(environment)
        actions["buttons"][:, 1, 5] = 1
        _, rewards, terminated, _, info = environment.step(actions)
        np.testing.assert_array_equal(
            rewards, np.asarray([1.0, 1.0], dtype=np.float64)
        )
        self.assertTrue(np.all(terminated))
        np.testing.assert_array_equal(
            info["player_rewards_q16"],
            np.tile(
                np.asarray([65536, -65536, 65536, -65536], dtype=np.int32),
                (2, 1),
            ),
        )

    def test_masked_reset_and_action_validation(self) -> None:
        environment = PlatformFighterVectorEnv(
            4, library_path=self.library_path, player_count=2
        )
        self.addCleanup(environment.close)
        initial, _ = environment.reset(seed=500)
        actions = _zero_actions(environment)
        actions["main_stick"][:, 0, 0] = 32767
        stepped, *_ = environment.step(actions)

        mask = np.asarray([False, True, False, True], dtype=np.bool_)
        reset, _ = environment.reset(
            seed=[1, 2, 3, 4], options={"reset_mask": mask}
        )
        self.assertTrue(np.array_equal(reset[~mask], stepped[~mask]))
        self.assertTrue(np.all(reset[mask, 0] == 0))
        self.assertFalse(np.array_equal(initial[mask], stepped[mask]))

        invalid = _zero_actions(environment)
        invalid["main_stick"] = invalid["main_stick"].astype(np.int32)
        with self.assertRaises(ValueError):
            environment.step(invalid)

        fresh = PlatformFighterVectorEnv(
            2, library_path=self.library_path, player_count=2
        )
        self.addCleanup(fresh.close)
        with self.assertRaises(RuntimeError):
            fresh.reset(
                seed=1,
                options={
                    "reset_mask": np.asarray(
                        [True, False], dtype=np.bool_
                    )
                },
            )

    def test_close_is_idempotent(self) -> None:
        environment = PlatformFighterVectorEnv(
            1, library_path=self.library_path
        )
        environment.reset(seed=1)
        environment.close()
        environment.close()
        with self.assertRaises(RuntimeError):
            environment.step(_zero_actions(environment))

    def test_current_native_button_mapping(self) -> None:
        environment = PlatformFighterVectorEnv(
            1, library_path=self.library_path, player_count=2
        )
        self.addCleanup(environment.close)
        environment.reset(seed=17)

        actions = _zero_actions(environment)
        actions["buttons"][0, 0, :5] = 1
        observation, _, terminated, _, _ = environment.step(actions)
        self.assertFalse(terminated[0])
        self.assertEqual(int(observation[0, 8]), 0x1F)
        self.assertEqual(int(observation[0, 9]), 0)

        actions = _zero_actions(environment)
        actions["buttons"][0, 0, 5] = 1
        _, _, terminated, _, _ = environment.step(actions)
        self.assertTrue(terminated[0])


if __name__ == "__main__":
    unittest.main()
