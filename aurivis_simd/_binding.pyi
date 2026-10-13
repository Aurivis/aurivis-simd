"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

from ._types import CpuFeatures, Level

class Binding:
    """Connects the C++ detection to the Python types.

    This class is internal. Use :class:`aurivis_simd.Cpu` instead.
    """

    @staticmethod
    def features() -> CpuFeatures:
        """Detects the SIMD features of the current machine.

        Returns
        --------
        :class:`CpuFeatures`
            The features that both the CPU and the operating system support.
        """
        ...

    @staticmethod
    def best_level() -> Level:
        """Gets the highest level the machine supports, ignoring ``AURIVIS_SIMD``.

        Returns
        --------
        :class:`Level`
            The best supported level.
        """
        ...

    @staticmethod
    def active_level() -> Level:
        """Gets the level that dispatch uses.

        Returns
        --------
        :class:`Level`
            The best supported level, lowered by ``AURIVIS_SIMD`` if set.
        """
        ...

__all__ = ['Binding']