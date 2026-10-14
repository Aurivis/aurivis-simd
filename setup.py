"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import sys

from setuptools import Extension, setup

class Build:
    """Describes the native extension of aurivis-simd.

    setuptools compiles ``.pyx`` sources with Cython automatically, because Cython is listed
    in the build requirements of ``pyproject.toml``.
    """

    __slots__ = ()

    INCLUDE_DIRECTORY = 'aurivis_simd/include'

    @staticmethod
    def compile_arguments() -> list[str]:
        """Gets the compiler flags for C++20 on the current platform.

        Returns
        --------
        List[:class:`str`]
            ``/std:c++20`` for MSVC, ``-std=c++20`` for GCC and Clang.
        """
        if sys.platform == 'win32':
            return ['/std:c++20']
        return ['-std=c++20']

    @staticmethod
    def extensions() -> list[Extension]:
        """Gets the native extensions to build.

        Returns
        --------
        List[:class:`Extension`]
            The ``aurivis_simd._binding`` extension.
        """
        binding = Extension(
            'aurivis_simd._binding',
            ['aurivis_simd/_binding.pyx'],
            include_dirs=[Build.INCLUDE_DIRECTORY],
            language='c++',
            extra_compile_args=Build.compile_arguments()
        )
        return [binding]

setup(ext_modules=Build.extensions())