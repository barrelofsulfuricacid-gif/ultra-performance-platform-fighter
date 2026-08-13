"""Shared physical-route declarations for Falcon's ordinary airborne poses."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class AirborneRoute:
    track_id: str
    source_action: str
    label_prefix: str
    frame_count: int
    successor: str


AIRBORNE_ROUTES = (
    AirborneRoute(
        "jump_forward", "JUMPING_FORWARD", "jump_forward_ecb", 35, "FALLING"
    ),
    AirborneRoute(
        "jump_backward", "JUMPING_BACKWARD", "jump_backward_ecb", 50, "FALLING"
    ),
    AirborneRoute(
        "jump_aerial_forward",
        "JUMPING_ARIAL_FORWARD",
        "jump_aerial_forward_ecb",
        50,
        "FALLING_AERIAL",
    ),
    AirborneRoute(
        "jump_aerial_backward",
        "JUMPING_ARIAL_BACKWARD",
        "jump_aerial_backward_ecb",
        35,
        "FALLING_AERIAL",
    ),
)

AIRBORNE_SUCCESSOR_TRACKS = (
    ("fall", "FALLING", "jump_forward_ecb_observe", 8),
    (
        "fall_aerial",
        "FALLING_AERIAL",
        "jump_aerial_forward_ecb_observe",
        8,
    ),
)
