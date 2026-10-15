"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import importlib
import importlib.util
import os
import sys
import tempfile
import unittest

from typing import Any, ClassVar

import aurivis_simd

class NativeBuild:
    """Builds the C++ test extension from ``tests/native`` once per test run."""

    __slots__ = ()

    DIRECTORY = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'native')

    @staticmethod
    def load() -> Any:
        """Compiles the extension into a temporary directory and imports it.

        Returns
        --------
        Any
            The ``NativeTests`` class of the compiled extension.
        """
        from setuptools import Extension, setup

        output = tempfile.mkdtemp(prefix='aurivis-simd-native-tests-')
        arguments = ['/std:c++20'] if sys.platform == 'win32' else ['-std=c++20', '-Werror=ignored-attributes']
        extension = Extension(
            '_native_tests',
            [os.path.join(NativeBuild.DIRECTORY, '_native_tests.pyx')],
            include_dirs=[str(aurivis_simd.Headers.get_include()), NativeBuild.DIRECTORY],
            language='c++',
            extra_compile_args=arguments
        )
        setup(
            name='aurivis-simd-native-tests',
            ext_modules=[extension],
            script_args=['--quiet', 'build_ext', '--build-lib', output, '--build-temp', os.path.join(output, 'temp')]
        )
        sys.path.insert(0, output)
        return importlib.import_module('_native_tests').NativeTests

class NativeTests(unittest.TestCase):
    """Runs the C++ tests of the headers. Needs Cython, setuptools and a C++20 compiler."""

    tests: ClassVar[Any] = None

    @classmethod
    def setUpClass(cls) -> None:
        if importlib.util.find_spec('Cython') is None or importlib.util.find_spec('setuptools') is None:
            raise unittest.SkipTest('Cython and setuptools are required to build the C++ tests')
        cls.tests = NativeBuild.load()

    def test_level_names_round_trip(self) -> None:
        self.assertEqual(NativeTests.tests.level_names_round_trip(), '')

    def test_support_rules(self) -> None:
        self.assertEqual(NativeTests.tests.support_rules(), '')

    def test_best_level_mapping(self) -> None:
        self.assertEqual(NativeTests.tests.best_level_mapping(), '')

    def test_detection_is_cached(self) -> None:
        self.assertEqual(NativeTests.tests.detection_is_cached(), '')

    def test_dispatch_fallback(self) -> None:
        self.assertEqual(NativeTests.tests.dispatch_fallback(), '')

    def test_dispatch_select(self) -> None:
        self.assertEqual(NativeTests.tests.dispatch_select(), '')

    def test_target_attributes(self) -> None:
        self.assertEqual(NativeTests.tests.target_attributes(), '')

    def test_aligned_buffer_basics(self) -> None:
        self.assertEqual(NativeTests.tests.aligned_buffer_basics(), '')

    def test_aligned_buffer_errors(self) -> None:
        self.assertEqual(NativeTests.tests.aligned_buffer_errors(), '')

    def test_pool_reuse(self) -> None:
        self.assertEqual(NativeTests.tests.pool_reuse(), '')

    def test_pool_lifetime_and_errors(self) -> None:
        self.assertEqual(NativeTests.tests.pool_lifetime_and_errors(), '')

    def test_pool_threads(self) -> None:
        self.assertEqual(NativeTests.tests.pool_threads(), '')

    def test_pool_cross_thread_return(self) -> None:
        self.assertEqual(NativeTests.tests.pool_cross_thread_return(), '')