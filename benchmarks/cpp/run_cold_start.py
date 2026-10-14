"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import glob
import os
import re
import statistics
import subprocess
import sys

class ColdStart:
    """Measures the first detection call of aurivis-simd and Highway in fresh processes.

    Usage: ``python run_cold_start.py [build directory]``
    """

    __slots__ = ()

    RUNS = 31
    PROGRAMS = (
        ('aurivis-simd  Detect()', 'BenchAurivis'),
        ('Highway       SupportedTargets()', 'BenchHighway')
    )

    @staticmethod
    def find(build_directory: str, name: str) -> str:
        """Finds a benchmark executable in a single- or multi-config CMake build directory."""
        for pattern in (f'{name}.exe', f'{name}', os.path.join('*', f'{name}.exe'), os.path.join('*', name)):
            matches = [path for path in glob.glob(os.path.join(build_directory, pattern)) if os.path.isfile(path)]
            if matches:
                return os.path.abspath(matches[0])
        raise FileNotFoundError(f'{name} not found in {build_directory}')

    @staticmethod
    def measure(executable: str) -> tuple[float, float]:
        """Runs the executable repeatedly and returns the median and minimum first-call time in nanoseconds."""
        samples: list[float] = []
        for _ in range(ColdStart.RUNS):
            output = subprocess.run([executable, '--cold'], capture_output=True, text=True, check=True).stdout
            found = re.search(r'first call\s+([\d.]+) ns', output)
            if found is None:
                raise RuntimeError(f'Unexpected output from {executable}: {output!r}')
            samples.append(float(found.group(1)))
        return statistics.median(samples), min(samples)

    @staticmethod
    def run(build_directory: str) -> None:
        """Prints the cold start results for both libraries."""
        print('[first call in a fresh process]')
        for label, name in ColdStart.PROGRAMS:
            median, best = ColdStart.measure(ColdStart.find(build_directory, name))
            print(f'  {label:<48} {median:10.0f} ns      (min {best:6.0f}, {ColdStart.RUNS} fresh processes)')

if __name__ == '__main__':
    ColdStart.run(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'build'))