"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import unittest

from aurivis_simd import CpuFeatures, Level

class LevelTests(unittest.TestCase):
    """Tests for :class:`Level`."""

    def test_values_match_cpp(self) -> None:
        self.assertEqual([level.value for level in Level], [0, 1, 2, 3, 16, 17, 18])

    def test_generic_runs_everywhere(self) -> None:
        for active in Level:
            self.assertTrue(Level.GENERIC.is_supported_by(active))

    def test_x86_levels_are_ordered(self) -> None:
        self.assertTrue(Level.SSE41.is_supported_by(Level.AVX2))
        self.assertTrue(Level.AVX2.is_supported_by(Level.AVX2))
        self.assertFalse(Level.AVX512.is_supported_by(Level.AVX2))
        self.assertFalse(Level.SSE41.is_supported_by(Level.GENERIC))

    def test_arm_levels_are_ordered(self) -> None:
        self.assertTrue(Level.NEON.is_supported_by(Level.SVE2))
        self.assertTrue(Level.SVE.is_supported_by(Level.SVE2))
        self.assertFalse(Level.SVE2.is_supported_by(Level.SVE))
        self.assertFalse(Level.SVE.is_supported_by(Level.NEON))

    def test_families_do_not_mix(self) -> None:
        self.assertFalse(Level.NEON.is_supported_by(Level.AVX512))
        self.assertFalse(Level.AVX2.is_supported_by(Level.NEON))
        self.assertTrue(Level.NEON.is_supported_by(Level.NEON))
        self.assertFalse(Level.SVE2.is_supported_by(Level.AVX512))
        self.assertFalse(Level.AVX512.is_supported_by(Level.SVE2))

    def test_is_arm(self) -> None:
        self.assertTrue(Level.NEON.is_arm)
        self.assertTrue(Level.SVE.is_arm)
        self.assertTrue(Level.SVE2.is_arm)
        self.assertFalse(Level.AVX512.is_arm)
        self.assertFalse(Level.GENERIC.is_arm)

class CpuFeaturesTests(unittest.TestCase):
    """Tests for :class:`CpuFeatures`."""

    X86_V2 = {'sse3': True, 'ssse3': True, 'sse41': True, 'sse42': True, 'popcnt': True}
    X86_V3 = X86_V2 | {
        'avx': True,
        'avx2': True,
        'fma': True,
        'bmi1': True,
        'bmi2': True,
        'f16c': True,
        'lzcnt': True,
        'movbe': True
    }
    X86_V4 = X86_V3 | {'avx512f': True, 'avx512bw': True, 'avx512cd': True, 'avx512dq': True, 'avx512vl': True}

    def test_defaults_are_false(self) -> None:
        features = CpuFeatures()
        for name in CpuFeatures.__slots__:
            self.assertIs(getattr(features, name), False)

    def test_best_level_follows_x86_64_levels(self) -> None:
        self.assertIs(CpuFeatures().best_level(), Level.GENERIC)
        self.assertIs(CpuFeatures(**CpuFeaturesTests.X86_V2).best_level(), Level.SSE41)
        self.assertIs(CpuFeatures(**CpuFeaturesTests.X86_V3).best_level(), Level.AVX2)
        self.assertIs(CpuFeatures(**CpuFeaturesTests.X86_V4).best_level(), Level.AVX512)

    def test_one_missing_feature_lowers_the_level(self) -> None:
        without_movbe = CpuFeaturesTests.X86_V3 | {'movbe': False}
        self.assertIs(CpuFeatures(**without_movbe).best_level(), Level.SSE41)
        without_vl = CpuFeaturesTests.X86_V4 | {'avx512vl': False}
        self.assertIs(CpuFeatures(**without_vl).best_level(), Level.AVX2)

    def test_arm_levels(self) -> None:
        self.assertIs(CpuFeatures(neon=True, dot_prod=True, i8mm=True).best_level(), Level.NEON)
        self.assertIs(CpuFeatures(neon=True, sve=True).best_level(), Level.SVE)
        self.assertIs(CpuFeatures(neon=True, sve=True, sve2=True).best_level(), Level.SVE2)
        self.assertIs(CpuFeatures(neon=True, sve2=True).best_level(), Level.NEON)

    def test_level_checks(self) -> None:
        features = CpuFeatures(**CpuFeaturesTests.X86_V3)
        self.assertTrue(features.has_x86_v2)
        self.assertTrue(features.has_x86_v3)
        self.assertFalse(features.has_x86_v4)

    def test_is_immutable(self) -> None:
        features = CpuFeatures(avx2=True)
        with self.assertRaises(AttributeError):
            features.avx2 = False
        with self.assertRaises(AttributeError):
            del features.avx2
        self.assertTrue(features.avx2)

    def test_equality_and_hash(self) -> None:
        first = CpuFeatures(**CpuFeaturesTests.X86_V3)
        second = CpuFeatures(**CpuFeaturesTests.X86_V3)
        self.assertEqual(first, second)
        self.assertEqual(hash(first), hash(second))
        self.assertNotEqual(first, CpuFeatures(**CpuFeaturesTests.X86_V2))
        self.assertNotEqual(first, object())

    def test_repr_lists_enabled_features(self) -> None:
        text = repr(CpuFeatures(avx2=True, fma=True))
        self.assertIn('avx2 fma', text)
        self.assertIn('Level.GENERIC', text)