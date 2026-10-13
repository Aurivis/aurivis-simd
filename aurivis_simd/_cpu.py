"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

from ._binding import Binding
from ._types import CpuFeatures, Level

class Cpu:
    """Gives access to the SIMD features of the current machine.

    Detection runs once when the module is imported. All methods return the cached result,
    so they are cheap to call.
    """

    __slots__ = ()

    _features = Binding.features()
    _best_level = Binding.best_level()
    _active_level = Binding.active_level()

    @staticmethod
    def features() -> CpuFeatures:
        """Gets the SIMD features of the current machine.

        Returns
        --------
        :class:`CpuFeatures`
            The features that both the CPU and the operating system support.
        """
        return Cpu._features

    @staticmethod
    def best_level() -> Level:
        """Gets the highest level the machine supports, ignoring ``AURIVIS_SIMD``.

        Returns
        --------
        :class:`Level`
            The best supported level.
        """
        return Cpu._best_level

    @staticmethod
    def active_level() -> Level:
        """Gets the level that dispatch uses.

        Returns
        --------
        :class:`Level`
            The best supported level, lowered by ``AURIVIS_SIMD`` if set.
        """
        return Cpu._active_level

    @staticmethod
    def supports(level: Level) -> bool:
        """Checks whether code built for a level can run on this machine.

        Parameters
        -----------
        level: :class:`Level`
            The level to check.

        Returns
        --------
        :class:`bool`
            ``True`` if ``level`` is supported by :meth:`active_level`.
        """
        return level.is_supported_by(Cpu._active_level)

__all__ = ['Cpu']