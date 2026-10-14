"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import os
import unittest

from aurivis_simd import Headers, IncludePath

class HeadersTests(unittest.TestCase):
    """Tests for :class:`Headers` and :class:`IncludePath`."""

    HEADERS = (
        'AlignedBuffer.hpp',
        'Config.hpp',
        'Cpu.hpp',
        'CpuFeatures.hpp',
        'Dispatch.hpp',
        'Level.hpp',
        'Pool.hpp',
        'Simd.hpp',
        os.path.join('Detail', 'AlignedAlloc.hpp'),
        os.path.join('Detail', 'CpuX86.hpp')
    )

    def test_include_directory_exists(self) -> None:
        self.assertTrue(Headers.get_include().exists)

    def test_all_headers_are_shipped(self) -> None:
        root = os.path.join(Headers.get_include(), 'Aurivis', 'Simd')
        for header in HeadersTests.HEADERS:
            self.assertTrue(os.path.isfile(os.path.join(root, header)), header)

    def test_is_path_like(self) -> None:
        path = Headers.get_include()
        self.assertIsInstance(path, os.PathLike)
        self.assertEqual(os.fspath(path), str(path))

    def test_equality_and_hash(self) -> None:
        self.assertEqual(Headers.get_include(), Headers.get_include())
        self.assertEqual(hash(Headers.get_include()), hash(Headers.get_include()))
        self.assertNotEqual(Headers.get_include(), IncludePath('elsewhere'))
        self.assertNotEqual(Headers.get_include(), object())

    def test_is_immutable(self) -> None:
        path = Headers.get_include()
        with self.assertRaises(AttributeError):
            setattr(path, '_path', 'elsewhere')
        with self.assertRaises(AttributeError):
            delattr(path, '_path')
        self.assertTrue(path.exists)