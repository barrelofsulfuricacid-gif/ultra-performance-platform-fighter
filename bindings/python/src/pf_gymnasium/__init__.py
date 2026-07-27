"""Gymnasium compatibility layer for the platform-fighter C simulation."""

from .vector_env import PlatformFighterError, PlatformFighterVectorEnv

__all__ = ["PlatformFighterError", "PlatformFighterVectorEnv"]
__version__ = "0.1.0"
