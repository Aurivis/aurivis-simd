"""
Copyright (c) 2026-present Aurivis

Licensed under the Apache License, Version 2.0. See LICENSE for details.
"""

from __future__ import annotations

import glob
import importlib.util
import os
import statistics
import subprocess
import sys
import timeit

class DetectionBenchmark:
    """Compares CPU feature detection from Python: aurivis-simd, NumPy and py-cpuinfo.

    NumPy and py-cpuinfo are optional. Libraries that are not installed are skipped.
    """

    __slots__ = ()

    ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
    COLD_RUNS = 15
    WARM_RUNS = 7
    LIBRARIES = (
        (
            'aurivis-simd',
            'aurivis_simd',
            'from aurivis_simd import Cpu\nCpu.features().avx2',
            'features.avx2',
            'from aurivis_simd import Cpu\nfeatures = Cpu.features()',
            5_000_000
        ),
        (
            'numpy',
            'numpy',
            "import numpy\nnumpy._core._multiarray_umath.__cpu_features__['AVX2']",
            "features['AVX2']",
            'import numpy\nfeatures = numpy._core._multiarray_umath.__cpu_features__',
            5_000_000
        ),
        (
            'py-cpuinfo',
            'cpuinfo',
            "import cpuinfo\ncpuinfo.get_cpu_info()['flags']",
            "cpuinfo.get_cpu_info()['flags']",
            'import cpuinfo',
            3
        )
    )

    @staticmethod
    def cold(snippet: str) -> tuple[float, float]:
        """Runs a snippet in fresh interpreters and returns the median and minimum time in milliseconds."""
        code = f'import time\nstart = time.perf_counter()\n{snippet}\nprint(time.perf_counter() - start)'
        command = [sys.executable, '-c', code]
        samples: list[float] = []
        for _ in range(DetectionBenchmark.COLD_RUNS):
            result = subprocess.run(command, capture_output=True, text=True, check=True, cwd=DetectionBenchmark.ROOT)
            samples.append(float(result.stdout))
        return statistics.median(samples) * 1e3, min(samples) * 1e3

    @staticmethod
    def warm(statement: str, setup: str, number: int) -> tuple[float, float]:
        """Times a statement in this interpreter and returns the median and minimum time in nanoseconds."""
        totals = timeit.repeat(statement, setup, number=number, repeat=DetectionBenchmark.WARM_RUNS)
        samples = [total / number * 1e9 for total in totals]
        return statistics.median(samples), min(samples)

    @staticmethod
    def format(value: float) -> str:
        """Formats a nanosecond value with a fitting unit."""
        if value >= 1e6:
            return f'{value / 1e6:10.2f} ms'
        if value >= 1e3:
            return f'{value / 1e3:10.2f} us'
        return f'{value:10.2f} ns'

    @staticmethod
    def run() -> None:
        """Prints the cold and warm results for every installed library.

        Uses the in-place build from ``setup.py build_ext --inplace`` if there is one, otherwise the installed package.
        """
        package = os.path.join(DetectionBenchmark.ROOT, 'aurivis_simd')
        if glob.glob(os.path.join(package, '_binding*.pyd')) or glob.glob(os.path.join(package, '_binding*.so')):
            sys.path.insert(0, DetectionBenchmark.ROOT)
        installed = [library for library in DetectionBenchmark.LIBRARIES if importlib.util.find_spec(library[1]) is not None]
        print(f'Python {sys.version.split()[0]} | ' + ', '.join(library[0] for library in installed))
        print('\n[cold: fresh process, import + first query]')
        for name, _, snippet, _, _, _ in installed:
            median, best = DetectionBenchmark.cold(snippet)
            print(f'  {name:<16} {median:10.2f} ms   (min {best:8.2f})')
        print('\n[warm: already imported, repeated query, result kept]')
        for name, _, _, statement, setup, number in installed:
            median, best = DetectionBenchmark.warm(statement, setup, number)
            print(f'  {name:<16} {DetectionBenchmark.format(median)}   (min {DetectionBenchmark.format(best).strip()})')

if __name__ == '__main__':
    DetectionBenchmark.run()