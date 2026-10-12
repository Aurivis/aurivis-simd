"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import enum

class Level(enum.Enum):
    """SIMD instruction set levels, ordered from slowest to fastest within each architecture family.

    The x86 levels follow the x86-64 microarchitecture levels v2, v3 and v4. Levels of
    different families must not be compared by value. Use :meth:`is_supported_by` instead.
    """

    _value_: int

    GENERIC = 0
    SSE41 = 1
    AVX2 = 2
    AVX512 = 3
    NEON = 16

    @property
    def is_arm(self) -> bool:
        """:class:`bool`: Whether this level belongs to the ARM family."""
        return self.value >= Level.NEON.value

    def is_supported_by(self, active: Level) -> bool:
        """Checks whether code built for this level can run when another level is active.

        Parameters
        -----------
        active: :class:`Level`
            The level the CPU allows, possibly lowered by ``AURIVIS_SIMD``.

        Returns
        --------
        :class:`bool`
            ``True`` if this level is :attr:`GENERIC`, or belongs to the same family as
            ``active`` and is not above it.
        """
        if self is Level.GENERIC:
            return True
        if self.is_arm != active.is_arm:
            return False
        rank: int = self.value
        return rank <= active.value

class CpuFeatures:
    """The SIMD features that are usable on the current machine.

    A feature is only reported when both the CPU and the operating system support it.
    Features of the other architecture family are always ``False``. Instances are immutable.

    .. container:: operations

        .. describe:: x == y

            Checks if two feature sets are equal.

        .. describe:: hash(x)

            Returns the hash of the feature set.

    Attributes
    -----------
    sse2, sse3, ssse3, sse41, sse42, popcnt: :class:`bool`
        x86 SSE family.
    avx, avx2, fma, bmi1, bmi2, f16c, lzcnt, movbe, avx_vnni, gfni, vaes, vpclmulqdq: :class:`bool`
        x86 AVX family.
    avx512f, avx512bw, avx512cd, avx512dq, avx512vl, avx512ifma, avx512vbmi, avx512vbmi2, avx512vnni, avx512bitalg, avx512vpopcntdq, avx512bf16, avx512fp16: :class:`bool`
        x86 AVX-512 family.
    neon, neon_fp16, neon_bf16, dot_prod, i8mm, sve, sve2: :class:`bool`
        ARM family.
    """

    __slots__ = (
        'sse2',
        'sse3',
        'ssse3',
        'sse41',
        'sse42',
        'popcnt',
        'avx',
        'avx2',
        'fma',
        'bmi1',
        'bmi2',
        'f16c',
        'lzcnt',
        'movbe',
        'avx_vnni',
        'gfni',
        'vaes',
        'vpclmulqdq',
        'avx512f',
        'avx512bw',
        'avx512cd',
        'avx512dq',
        'avx512vl',
        'avx512ifma',
        'avx512vbmi',
        'avx512vbmi2',
        'avx512vnni',
        'avx512bitalg',
        'avx512vpopcntdq',
        'avx512bf16',
        'avx512fp16',
        'neon',
        'neon_fp16',
        'neon_bf16',
        'dot_prod',
        'i8mm',
        'sve',
        'sve2'
    )

    sse2: bool
    sse3: bool
    ssse3: bool
    sse41: bool
    sse42: bool
    popcnt: bool
    avx: bool
    avx2: bool
    fma: bool
    bmi1: bool
    bmi2: bool
    f16c: bool
    lzcnt: bool
    movbe: bool
    avx_vnni: bool
    gfni: bool
    vaes: bool
    vpclmulqdq: bool
    avx512f: bool
    avx512bw: bool
    avx512cd: bool
    avx512dq: bool
    avx512vl: bool
    avx512ifma: bool
    avx512vbmi: bool
    avx512vbmi2: bool
    avx512vnni: bool
    avx512bitalg: bool
    avx512vpopcntdq: bool
    avx512bf16: bool
    avx512fp16: bool
    neon: bool
    neon_fp16: bool
    neon_bf16: bool
    dot_prod: bool
    i8mm: bool
    sve: bool
    sve2: bool

    def __init__(
        self,
        *,
        sse2: bool = False,
        sse3: bool = False,
        ssse3: bool = False,
        sse41: bool = False,
        sse42: bool = False,
        popcnt: bool = False,
        avx: bool = False,
        avx2: bool = False,
        fma: bool = False,
        bmi1: bool = False,
        bmi2: bool = False,
        f16c: bool = False,
        lzcnt: bool = False,
        movbe: bool = False,
        avx_vnni: bool = False,
        gfni: bool = False,
        vaes: bool = False,
        vpclmulqdq: bool = False,
        avx512f: bool = False,
        avx512bw: bool = False,
        avx512cd: bool = False,
        avx512dq: bool = False,
        avx512vl: bool = False,
        avx512ifma: bool = False,
        avx512vbmi: bool = False,
        avx512vbmi2: bool = False,
        avx512vnni: bool = False,
        avx512bitalg: bool = False,
        avx512vpopcntdq: bool = False,
        avx512bf16: bool = False,
        avx512fp16: bool = False,
        neon: bool = False,
        neon_fp16: bool = False,
        neon_bf16: bool = False,
        dot_prod: bool = False,
        i8mm: bool = False,
        sve: bool = False,
        sve2: bool = False
    ):
        values = locals()
        for name in CpuFeatures.__slots__:
            object.__setattr__(self, name, values[name])

    def __setattr__(self, name: str, value: object) -> None:
        raise AttributeError(f'{self.__class__.__name__} is immutable')

    def __delattr__(self, name: str) -> None:
        raise AttributeError(f'{self.__class__.__name__} is immutable')

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, CpuFeatures):
            return NotImplemented
        return all(getattr(self, name) == getattr(other, name) for name in CpuFeatures.__slots__)

    def __hash__(self) -> int:
        return hash(tuple(getattr(self, name) for name in CpuFeatures.__slots__))

    def __repr__(self) -> str:
        enabled = ' '.join(name for name in CpuFeatures.__slots__ if getattr(self, name))
        return f'<CpuFeatures best_level={self.best_level()!r} enabled={enabled!r}>'

    @property
    def has_x86_v2(self) -> bool:
        """:class:`bool`: Whether the x86-64-v2 requirements of :attr:`Level.SSE41` are met."""
        return self.sse3 and self.ssse3 and self.sse41 and self.sse42 and self.popcnt

    @property
    def has_x86_v3(self) -> bool:
        """:class:`bool`: Whether the x86-64-v3 requirements of :attr:`Level.AVX2` are met."""
        return self.has_x86_v2 and self.avx and self.avx2 and self.fma and self.bmi1 and self.bmi2 and self.f16c and self.lzcnt and self.movbe

    @property
    def has_x86_v4(self) -> bool:
        """:class:`bool`: Whether the x86-64-v4 requirements of :attr:`Level.AVX512` are met."""
        return self.has_x86_v3 and self.avx512f and self.avx512bw and self.avx512cd and self.avx512dq and self.avx512vl

    def best_level(self) -> Level:
        """Gets the highest level whose requirements are all met.

        Returns
        --------
        :class:`Level`
            :attr:`Level.NEON` on ARM, the matching x86 level, or :attr:`Level.GENERIC`.
        """
        if self.neon:
            return Level.NEON
        if self.has_x86_v4:
            return Level.AVX512
        if self.has_x86_v3:
            return Level.AVX2
        if self.has_x86_v2:
            return Level.SSE41
        return Level.GENERIC

__all__ = (
    'CpuFeatures',
    'Level'
)