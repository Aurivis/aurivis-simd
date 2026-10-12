# Copyright (c) 2026-present Aurivis
#
# Licensed under the Apache License, Version 2.0. See LICENSE for details.

from libc.stdint cimport uint8_t
from libcpp cimport bool
from libcpp.string_view cimport string_view

cdef extern from 'Aurivis/Simd/Simd.hpp' namespace 'Aurivis::Simd' nogil:
    enum class Level(uint8_t):
        Generic
        Sse41
        Avx2
        Avx512
        Neon

    cppclass CpuFeatures:
        bool sse2
        bool sse3
        bool ssse3
        bool sse41
        bool sse42
        bool popcnt
        bool avx
        bool avx2
        bool fma
        bool bmi1
        bool bmi2
        bool f16c
        bool lzcnt
        bool movbe
        bool avxVnni
        bool gfni
        bool vaes
        bool vpclmulqdq
        bool avx512f
        bool avx512bw
        bool avx512cd
        bool avx512dq
        bool avx512vl
        bool avx512ifma
        bool avx512vbmi
        bool avx512vbmi2
        bool avx512vnni
        bool avx512bitalg
        bool avx512vpopcntdq
        bool avx512bf16
        bool avx512fp16
        bool neon
        bool neonFp16
        bool neonBf16
        bool dotProd
        bool i8mm
        bool sve
        bool sve2
        bool HasX86V2()
        bool HasX86V3()
        bool HasX86V4()
        Level GetBestLevel()

    const CpuFeatures& Detect()
    Level GetBestLevel()
    Level GetActiveLevel()
    bool IsSupportedBy(Level candidate, Level active)
    const char* LevelToString(Level level)
    bool TryParseLevel(string_view text, Level& outLevel)

    const size_t DEFAULT_ALIGNMENT

    cppclass AlignedBuffer[T]:
        AlignedBuffer()
        AlignedBuffer(size_t count) except +
        AlignedBuffer(size_t count, size_t alignment) except +
        T* GetData()
        size_t GetCount()
        size_t GetSizeInBytes()
        size_t GetAlignment()
        bool IsEmpty()
        T& operator[](size_t index)
        void Reset()

    cppclass Pool[T]:
        cppclass Buffer:
            Buffer()
            T* GetData()
            size_t GetCount()
            size_t GetSizeInBytes()
            bool IsEmpty()
            T& operator[](size_t index)
            void Reset()

        Pool(size_t count) except +
        Pool(size_t count, size_t alignment) except +
        Buffer Acquire() except +
        void Reserve(size_t bufferCount) except +
        void ReleaseIdle()
        size_t GetCount()
        size_t GetAlignment()
        size_t GetIdleCount()

    cppclass DispatchTable[Fn]:
        Fn generic
        Fn sse41
        Fn avx2
        Fn avx512
        Fn neon
        Fn Select()
        Fn SelectFor(Level level)