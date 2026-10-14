<div align="center">

# aurivis-simd

**CPU feature detection, runtime dispatch and aligned memory for SIMD code.**<br>
Header-only C++20 with a small, fully typed Python API. No runtime dependencies.

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
- Allocates SIMD-aligned memory with RAII buffers and a sharded, thread-safe buffer pool
- Ships its headers inside the wheel, so other extensions can build against them
- Provides Cython declarations for `cimport`
- Fully typed Python API (`py.typed`, passes `mypy --strict`)

## Status

| Feature | Status |
|---|---|
| x86 detection (SSE2 to AVX-512) | ✅ |
| Dispatch tables | ✅ |
| `AlignedBuffer`, `Pool` | ✅ |
| Python API | ✅ |
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

Intel Core i7-11700F, Windows 11, MSVC 2022, Python 3.14, Release build. Median of 9 runs.

### Detection

Compared with [Google Highway](https://github.com/google/highway) 1.4.0.

| Operation | aurivis-simd | Highway |
|---|---|---|
| Repeated feature query | **≈ 0 ns** ¹ | 341 ns ² |
| Detection without cache | **331 ns** | 341 ns |
| First call in a fresh process | 2.1 µs | **0.6 µs** ³ |

### Dispatch

| Operation | aurivis-simd | Highway |
|---|---|---|
| Select on every call | **1.17 ns** | 1.41 ns |
| Cached function pointer | **0.93 ns** | 0.94 ns |
| Direct call, for reference | 0.70 ns | 0.70 ns |

### Aligned memory

| Operation | `AlignedBuffer` | `Pool::Acquire()` | `hwy::AllocateAligned` |
|---|---|---|---|
| 2 KB | 35 ns | **13.8 ns** | 48 ns |
| 64 KB | 93 ns | **13.8 ns** | 125 ns |
| 1080p frame (3 MB) | 7.0 µs | **13.8 ns** | 7.2 µs |
| 2 KB, 8 threads at once ⁴ | 41 ns | **2.2 ns** | – |

### Python

Compared with [NumPy](https://numpy.org) 2.5 and [py-cpuinfo](https://github.com/workhorsy/py-cpuinfo) 9.0.

| Operation | aurivis-simd | NumPy | py-cpuinfo |
|---|---|---|---|
| Import and first query, fresh process | **10.4 ms** | 102 ms | 1,696 ms |
| Repeated query, result kept | **9.8 ns** | 20.4 ns | 1,590 ms |

¹ Not measurable: as fast as an empty loop iteration (1.6 ns).<br>
² `hwy::SupportedTargets()` runs the detection again on every call. Highway caches its dispatch choice separately.<br>
³ The difference comes from the operating system mapping the code page on first use (about 1 µs), not from the detection itself. It is paid once per process.<br>
⁴ Throughput of all threads together. Each thread acquires, writes and returns a buffer in a loop.

## Installation

```bash
pip install aurivis-simd
```

## Python

```python
from aurivis_simd import Cpu, Level

Cpu.best_level()
# Level.AVX512

Cpu.active_level()
# Level.AVX2 if AURIVIS_SIMD=avx2 is set, otherwise the same as best_level()

Cpu.supports(Level.AVX2)
# True

features = Cpu.features()
features.avx2
# True
```

Detection runs once when the package is imported, so every call returns a cached result. In hot loops, keep the `CpuFeatures` object instead of calling `Cpu.features()` again.

`CpuFeatures` and `IncludePath` are immutable. Every public name is typed, so IDEs and type checkers know all attributes and return types.

## Building against the headers

Add `aurivis-simd` to your build requirements:

```toml
[build-system]
requires = ["setuptools", "Cython", "aurivis-simd"]
```

Then pass the include directory to your extension. setuptools compiles `.pyx` sources with Cython automatically:

```python
import aurivis_simd
from setuptools import Extension, setup

setup(
    ext_modules=[
        Extension(
            'mypackage._native',
            ['mypackage/_native.pyx'],
            include_dirs=[str(aurivis_simd.Headers.get_include())],
            language='c++',
            extra_compile_args=['/std:c++20']
        )
    ]
)
```

Use `-std=c++20` instead of `/std:c++20` with GCC and Clang. The headers are compiled into your extension, so `aurivis-simd` is not needed at runtime.

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

AURIVIS_TARGET_AVX2 void TransformAvx2(const int16_t* input, uint8_t* output, size_t stride);

static const Aurivis::Simd::DispatchTable<TransformFn> table{
    .generic = &TransformGeneric,
    .sse41 = &TransformSse41,
    .avx2 = &TransformAvx2,
};

table.Select()(input, output, stride);
```

`Select()` returns the best entry that is not above the active level. Missing entries fall back to the next lower level, and finally to `generic`, which is required. The choice is cached in the table after the first call, so calling `Select()` every time is cheap. Tables cannot be copied, so declare them once as `static const`.

Mark each implementation with its `AURIVIS_TARGET_*` macro, so GCC and Clang allow the matching intrinsics without compiling the whole file for that level.

To test a specific implementation, use `SelectFor()`:

```cpp
const TransformFn fallback = table.SelectFor(Aurivis::Simd::Level::Generic);
```

### Aligned memory

```cpp
Aurivis::Simd::AlignedBuffer<int16_t> coefficients(32 * 32);
int16_t* data = coefficients.GetData();
```

Buffers are 64-byte aligned by default and can be moved but not copied. The memory is not initialized. Invalid alignments throw `std::invalid_argument`, failed allocations throw `std::bad_alloc`.

### Buffer pool

```cpp
Aurivis::Simd::Pool<uint8_t> framePool(1920 * 1080 * 3 / 2);
framePool.Reserve(8);

auto frame = framePool.Acquire();
```

The buffer goes back to the pool when `frame` goes out of scope, or when you call `frame.Reset()`. `Reserve()` allocates buffers in advance, so playback never waits for the operating system.

`Acquire()` is thread-safe, and buffers may be returned from any thread. Each thread works on its own shard, so threads rarely wait for each other. A buffer may outlive its pool; it is then freed when it is destroyed.

## Cython

```cython
# distutils: language = c++

from libcpp.utility cimport move

from aurivis_simd.simd cimport AlignedBuffer, GetActiveLevel, Level

if GetActiveLevel() == Level.Avx2:
    ...

cdef AlignedBuffer[float] buffer
buffer = move(AlignedBuffer[float](1024))
```

`AlignedBuffer` and `Pool.Buffer` can only be moved, so assign them with `move(...)`. C++ exceptions become Python exceptions: `std::invalid_argument` raises `ValueError`, `std::bad_alloc` raises `MemoryError`.

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