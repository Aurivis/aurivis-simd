"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import os
import platform
import subprocess
import sys
import unittest

from aurivis_simd import Cpu, CpuFeatures, Level

class CpuTests(unittest.TestCase):
    """Tests for :class:`Cpu` on the machine that runs the tests."""

    def test_features_type(self) -> None:
        self.assertIsInstance(Cpu.features(), CpuFeatures)

    def test_results_are_cached(self) -> None:
        self.assertIs(Cpu.features(), Cpu.features())
        self.assertIs(Cpu.best_level(), Cpu.best_level())

    def test_best_level_matches_features(self) -> None:
        self.assertIs(Cpu.best_level(), Cpu.features().best_level())

    def test_active_level_is_never_above_best_level(self) -> None:
        self.assertTrue(Cpu.active_level().is_supported_by(Cpu.best_level()))

    def test_supports(self) -> None:
        self.assertTrue(Cpu.supports(Level.GENERIC))
        self.assertTrue(Cpu.supports(Cpu.active_level()))
        other_family = Level.AVX2 if Cpu.best_level().is_arm else Level.NEON
        self.assertFalse(Cpu.supports(other_family))

    def test_baseline_features(self) -> None:
        machine = platform.machine().lower()
        if machine in ('amd64', 'x86_64'):
            self.assertTrue(Cpu.features().sse2)
            self.assertFalse(Cpu.features().neon)
        elif machine in ('arm64', 'aarch64'):
            self.assertTrue(Cpu.features().neon)
            self.assertFalse(Cpu.features().sse2)
        else:
            self.skipTest(f'No baseline known for {machine}')

class ArmDetectionTests(unittest.TestCase):
    """Compares the ARM detection with what the operating system reports through other channels."""

    @staticmethod
    def is_arm64() -> bool:
        return platform.machine().lower() in ('arm64', 'aarch64')

    def test_features_are_consistent(self) -> None:
        features = Cpu.features()
        extensions = (features.neon_fp16, features.neon_bf16, features.dot_prod, features.i8mm, features.sve, features.sve2)
        if not ArmDetectionTests.is_arm64():
            self.assertFalse(features.neon or any(extensions))
            return
        self.assertTrue(features.neon)
        self.assertTrue(features.sve or not features.sve2)

    def test_linux_matches_proc_cpuinfo(self) -> None:
        if sys.platform != 'linux' or not ArmDetectionTests.is_arm64():
            self.skipTest('Only on Linux on ARM64')
        with open('/proc/cpuinfo', encoding='utf-8') as file:
            line = next((line for line in file if line.startswith('Features')), '')
        flags = set(line.partition(':')[2].split())
        if not flags:
            self.skipTest('/proc/cpuinfo has no Features line')
        features = Cpu.features()
        self.assertEqual(features.neon_fp16, 'fphp' in flags and 'asimdhp' in flags)
        self.assertEqual(features.neon_bf16, 'bf16' in flags)
        self.assertEqual(features.dot_prod, 'asimddp' in flags)
        self.assertEqual(features.i8mm, 'i8mm' in flags)
        self.assertEqual(features.sve, 'sve' in flags)
        self.assertEqual(features.sve2, 'sve2' in flags)

    def test_apple_silicon(self) -> None:
        if sys.platform != 'darwin' or not ArmDetectionTests.is_arm64():
            self.skipTest('Only on Apple Silicon')
        if int(platform.mac_ver()[0].split('.')[0]) < 12:
            self.skipTest('macOS 11 does not report the detailed ARM features')
        features = Cpu.features()
        self.assertTrue(features.dot_prod)
        self.assertTrue(features.neon_fp16)
        self.assertFalse(features.sve)

class LevelLimitTests(unittest.TestCase):
    """Tests for the ``AURIVIS_SIMD`` environment variable.

    Every test starts a fresh interpreter, because the limit is read only once per process.
    """

    @staticmethod
    def active_level_with(value: str) -> Level:
        environment = os.environ | {'AURIVIS_SIMD': value}
        command = [sys.executable, '-c', 'from aurivis_simd import Cpu; print(Cpu.active_level().name)']
        result = subprocess.run(command, capture_output=True, text=True, check=True, env=environment)
        return Level[result.stdout.strip()]

    def test_generic_limit(self) -> None:
        self.assertIs(LevelLimitTests.active_level_with('generic'), Level.GENERIC)

    def test_limit_is_case_insensitive(self) -> None:
        self.assertIs(LevelLimitTests.active_level_with('GENERIC'), Level.GENERIC)

    def test_lower_limit_is_applied(self) -> None:
        if not Level.SSE41.is_supported_by(Cpu.best_level()):
            self.skipTest('This machine does not support SSE4.1')
        self.assertIs(LevelLimitTests.active_level_with('sse41'), Level.SSE41)

    def test_neon_limit_on_sve(self) -> None:
        if Cpu.best_level() not in (Level.SVE, Level.SVE2):
            self.skipTest('This machine does not support SVE')
        self.assertIs(LevelLimitTests.active_level_with('neon'), Level.NEON)

    def test_limit_never_raises_the_level(self) -> None:
        self.assertIs(LevelLimitTests.active_level_with('avx512'), Cpu.best_level())

    def test_other_family_is_ignored(self) -> None:
        other_family = 'avx2' if Cpu.best_level().is_arm else 'neon'
        self.assertIs(LevelLimitTests.active_level_with(other_family), Cpu.best_level())

    def test_invalid_values_are_ignored(self) -> None:
        self.assertIs(LevelLimitTests.active_level_with('bogus'), Cpu.best_level())
        self.assertIs(LevelLimitTests.active_level_with('a' * 64), Cpu.best_level())