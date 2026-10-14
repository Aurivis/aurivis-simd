"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from libcpp.string cimport string

cdef extern from 'NativeTests.hpp' namespace 'Aurivis::Simd::Tests':
    string LevelNamesRoundTrip() except +
    string SupportRules() except +
    string BestLevelMapping() except +
    string DetectionIsCached() except +
    string DispatchFallback() except +
    string DispatchSelect() except +
    string AlignedBufferBasics() except +
    string AlignedBufferErrors() except +
    string PoolReuse() except +
    string PoolLifetimeAndErrors() except +
    string PoolThreads() except +
    string PoolCrossThreadReturn() except +

class NativeTests:
    """Runs the C++ tests from ``NativeTests.hpp``.

    Every method returns an empty string on success, otherwise the condition that failed.
    """

    @staticmethod
    def level_names_round_trip() -> str:
        return LevelNamesRoundTrip().decode()

    @staticmethod
    def support_rules() -> str:
        return SupportRules().decode()

    @staticmethod
    def best_level_mapping() -> str:
        return BestLevelMapping().decode()

    @staticmethod
    def detection_is_cached() -> str:
        return DetectionIsCached().decode()

    @staticmethod
    def dispatch_fallback() -> str:
        return DispatchFallback().decode()

    @staticmethod
    def dispatch_select() -> str:
        return DispatchSelect().decode()

    @staticmethod
    def aligned_buffer_basics() -> str:
        return AlignedBufferBasics().decode()

    @staticmethod
    def aligned_buffer_errors() -> str:
        return AlignedBufferErrors().decode()

    @staticmethod
    def pool_reuse() -> str:
        return PoolReuse().decode()

    @staticmethod
    def pool_lifetime_and_errors() -> str:
        return PoolLifetimeAndErrors().decode()

    @staticmethod
    def pool_threads() -> str:
        return PoolThreads().decode()

    @staticmethod
    def pool_cross_thread_return() -> str:
        return PoolCrossThreadReturn().decode()

__all__ = ['NativeTests']