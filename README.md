<div align="center">

# aurivis-simd

**CPU feature detection, runtime dispatch and aligned memory for SIMD code.**<br>
Header-only C++20 with a small Python API. No runtime dependencies.

<br>

![Python](https://img.shields.io/badge/Python-3.11%2B-3776AB?style=for-the-badge&logo=python&logoColor=white&labelColor=1F2937)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white&labelColor=1F2937)
![Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20macOS%20%7C%20Linux-6E40C9?style=for-the-badge&labelColor=1F2937)
![Dependencies](https://img.shields.io/badge/Dependencies-none-2EA44F?style=for-the-badge&labelColor=1F2937)
[![License](https://img.shields.io/badge/License-Apache%202.0-D22128?style=for-the-badge&logo=apache&logoColor=white&labelColor=1F2937)](LICENSE)

</div>

<br>

## Features

- Detects CPU features at runtime, including whether the operating system allows AVX and AVX-512 registers
- Picks the fastest available implementation of a function with a dispatch table
- Lets you limit the SIMD level with the `AURIVIS_SIMD` environment variable for testing and bug reports
- Allocates SIMD-aligned memory with RAII buffers and a thread-safe buffer pool
- Ships its headers inside the wheel, so other extensions can build against them
- Provides Cython declarations for `cimport`

## Status

| Feature | Status |
|---|---|
| x86 detection (SSE2 to AVX-512) | ✅ |
| Dispatch tables | ❌ |
| `AlignedBuffer`, `Pool` | ❌ |
| Python API | ❌ |
| ARM detection (NEON, dotprod, SVE, SVE2) | ❌ |
| Portable vector types (`Vec128`, `Vec256`, `Vec512`) | ❌ |

## Levels

| Level | Required features |
|---|---|
| `Generic` | none |
| `Sse41` | x86-64-v2: SSE3, SSSE3, SSE4.1, SSE4.2, POPCNT |
| `Avx2` | x86-64-v3: AVX, AVX2, FMA, BMI1, BMI2, F16C, LZCNT, MOVBE |
| `Avx512` | x86-64-v4: AVX-512 F, BW, CD, DQ, VL |
| `Neon` | ARMv8 Advanced SIMD |

## Benchmarks

Compared with [Google Highway](https://github.com/google/highway) 1.4.0.
Intel Core i7-11700F, Windows 11, MSVC 2022, Release build. Median of 9 runs.

| Operation | aurivis-simd | Highway |
|---|---|---|
| Repeated feature query | **≈ 0 ns** ¹ | 341 ns ² |
| Detection without cache | **331 ns** | 341 ns |
| First call in a fresh process | 2.1 µs | **0.6 µs** ³ |

¹ Not measurable: as fast as an empty loop iteration (1.6 ns).<br>
² `hwy::SupportedTargets()` runs the detection again on every call. Highway caches its dispatch choice separately.<br>
³ The difference comes from the operating system mapping the code page on first use (about 1 µs), not from the detection itself. It is paid once per process.

## Installation

```bash
pip install aurivis-simd
```

## Python

```python
import aurivis_simd

aurivis_simd.detect()
# Level.AVX2

aurivis_simd.active_level()
# Level.AVX2, or lower if AURIVIS_SIMD is set

features = aurivis_simd.features()
features.avx2
# True
```

## Building against the headers

Add `aurivis-simd` to your build requirements:

```toml
[build-system]
requires = ["setuptools", "Cython", "aurivis-simd"]
```

Then pass the include directory to your extension:

```python
from setuptools import Extension, setup

import aurivis_simd

setup(
    ext_modules=[
        Extension(
            "mypackage._native",
            ["mypackage/_native.pyx"],
            include_dirs=[str(aurivis_simd.get_include())],
            language="c++",
        )
    ]
)
```

The headers are compiled into your extension, so `aurivis-simd` is not needed at runtime.

## C++

### Detection

```cpp
#include "Aurivis/Simd/Simd.hpp"

const Aurivis::Simd::CpuFeatures& features = Aurivis::Simd::Detect();
const Aurivis::Simd::Level best = Aurivis::Simd::GetBestLevel();
const Aurivis::Simd::Level active = Aurivis::Simd::GetActiveLevel();
```

`Detect()` runs once and caches the result. `GetBestLevel()` is the highest level the machine supports. `GetActiveLevel()` is the level dispatch uses, lowered by `AURIVIS_SIMD` if set. All three are thread-safe.

### Dispatch

```cpp
using TransformFn = void (*)(const int16_t* input, uint8_t* output, size_t stride);

const Aurivis::Simd::DispatchTable<TransformFn> table{
    .generic = &TransformGeneric,
    .sse41 = &TransformSse41,
    .avx2 = &TransformAvx2,
};

const TransformFn transform = table.Select();
```

`Select()` returns the best entry that is not above the active level. Missing entries fall back to the next lower level, and finally to `generic`.

### Aligned memory

```cpp
Aurivis::Simd::AlignedBuffer<int16_t> coefficients(32 * 32);
int16_t* data = coefficients.GetData();
```

Buffers are 64-byte aligned by default.

### Buffer pool

```cpp
Aurivis::Simd::Pool<uint8_t> framePool(1920 * 1080 * 3 / 2);

auto frame = framePool.Acquire();
```

The buffer goes back to the pool when `frame` goes out of scope. `Acquire()` is thread-safe.

## Cython

```cython
# distutils: language = c++

from aurivis_simd.simd cimport GetActiveLevel, Level

if GetActiveLevel() == Level.Avx2:
    ...
```

## Limiting the SIMD level

```bash
AURIVIS_SIMD=generic python app.py
```

Accepted values: `generic`, `sse41`, `avx2`, `avx512`, `neon`. The variable can only lower the level, never raise it above what the CPU supports. Invalid values are ignored.

## Requirements

- Python 3.11 or newer
- Windows, macOS or Linux on x86_64 or arm64
- To build against the headers: a C++20 compiler (MSVC 2022, GCC 10, Clang 12 or newer)

## Part of Aurivis

`aurivis-simd` is one of the native libraries behind the Aurivis media player. It works on its own and does not depend on Aurivis.

## License

Apache-2.0, see [LICENSE](LICENSE).