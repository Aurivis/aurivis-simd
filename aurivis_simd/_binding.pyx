"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from .simd cimport CpuFeatures as NativeCpuFeatures
from .simd cimport Detect, GetActiveLevel, GetBestLevel

from ._types import CpuFeatures, Level

cdef class Binding:
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
        cdef const NativeCpuFeatures* native = &Detect()
        return CpuFeatures(
            sse2=native.sse2,
            sse3=native.sse3,
            ssse3=native.ssse3,
            sse41=native.sse41,
            sse42=native.sse42,
            popcnt=native.popcnt,
            avx=native.avx,
            avx2=native.avx2,
            fma=native.fma,
            bmi1=native.bmi1,
            bmi2=native.bmi2,
            f16c=native.f16c,
            lzcnt=native.lzcnt,
            movbe=native.movbe,
            avx_vnni=native.avxVnni,
            gfni=native.gfni,
            vaes=native.vaes,
            vpclmulqdq=native.vpclmulqdq,
            avx512f=native.avx512f,
            avx512bw=native.avx512bw,
            avx512cd=native.avx512cd,
            avx512dq=native.avx512dq,
            avx512vl=native.avx512vl,
            avx512ifma=native.avx512ifma,
            avx512vbmi=native.avx512vbmi,
            avx512vbmi2=native.avx512vbmi2,
            avx512vnni=native.avx512vnni,
            avx512bitalg=native.avx512bitalg,
            avx512vpopcntdq=native.avx512vpopcntdq,
            avx512bf16=native.avx512bf16,
            avx512fp16=native.avx512fp16,
            neon=native.neon,
            neon_fp16=native.neonFp16,
            neon_bf16=native.neonBf16,
            dot_prod=native.dotProd,
            i8mm=native.i8mm,
            sve=native.sve,
            sve2=native.sve2
        )

    @staticmethod
    def best_level() -> Level:
        """Gets the highest level the machine supports, ignoring ``AURIVIS_SIMD``.

        Returns
        --------
        :class:`Level`
            The best supported level.
        """
        return Level(<int>GetBestLevel())

    @staticmethod
    def active_level() -> Level:
        """Gets the level that dispatch uses.

        Returns
        --------
        :class:`Level`
            The best supported level, lowered by ``AURIVIS_SIMD`` if set.
        """
        return Level(<int>GetActiveLevel())

__all__ = ['Binding']