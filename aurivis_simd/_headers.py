"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import os

class IncludePath:
    """A directory with C++ headers, usable wherever a path is accepted.

    Implements :class:`os.PathLike`, so it works with :func:`os.fspath`, :class:`pathlib.Path`
    and :func:`open`. Instances are immutable.

    .. container:: operations

        .. describe:: str(x)

            Returns the directory as a string, e.g. for ``include_dirs`` in ``setup.py``.

        .. describe:: os.fspath(x)

            Returns the directory as a string.

        .. describe:: x == y

            Checks if two include paths point to the same directory string.

        .. describe:: hash(x)

            Returns the hash of the include path.
    """

    __slots__ = ['_path']

    _path: str

    def __init__(self, path: str):
        object.__setattr__(self, '_path', path)

    def __setattr__(self, name: str, value: object) -> None:
        raise AttributeError(f'{self.__class__.__name__} is immutable')

    def __delattr__(self, name: str) -> None:
        raise AttributeError(f'{self.__class__.__name__} is immutable')

    def __fspath__(self) -> str:
        return self._path

    def __str__(self) -> str:
        return self._path

    def __repr__(self) -> str:
        return f'<IncludePath path={self._path!r}>'

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, IncludePath):
            return NotImplemented
        return self._path == other._path

    def __hash__(self) -> int:
        return hash(self._path)

    @property
    def exists(self) -> bool:
        """:class:`bool`: Whether the directory exists."""
        return os.path.isdir(self._path)

class Headers:
    """Gives access to the C++ headers that ship with aurivis-simd."""

    __slots__ = ()

    @staticmethod
    def get_include() -> IncludePath:
        """Gets the directory to pass to the compiler as an include path.

        Returns
        --------
        :class:`IncludePath`
            The ``include`` directory inside the installed package.
        """
        return IncludePath(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'include'))

__all__ = (
    'Headers',
    'IncludePath'
)