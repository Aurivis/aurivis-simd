<div align="center">

# aurivis-simd

**CPU feature detection, runtime dispatch and aligned memory for SIMD code.**<br>
Header-only C++20.

<br>

![Python](https://img.shields.io/badge/Python-3.11%2B-3776AB?style=for-the-badge&logo=python&logoColor=white&labelColor=1F2937)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white&labelColor=1F2937)
![Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20macOS%20%7C%20Linux-6E40C9?style=for-the-badge&labelColor=1F2937)
[![License](https://img.shields.io/badge/License-Apache%202.0-D22128?style=for-the-badge&logo=apache&logoColor=white&labelColor=1F2937)](LICENSE)

</div>

<br>

## Why this exists

Modern CPUs can process many values with a single instruction. These are the SIMD instruction sets: SSE, AVX2, AVX-512 on Intel and AMD, NEON and SVE on ARM. Used well, they make code like video decoding, audio processing or parsing several times faster.

The catch is that not every CPU has the same instructions. If your program uses AVX-512 on a machine that doesn't support it, it crashes. So most projects either build for the oldest CPU and leave a lot of speed on the table, or ship separate builds for every machine.

aurivis-simd solves this at runtime. You write the important parts of your code in a few versions, for example a plain one, one for AVX2 and one for AVX-512. aurivis-simd checks what the CPU and the operating system support and picks the fastest version that will actually run. One build works everywhere: an old laptop gets the plain version, a new workstation gets AVX-512.

It also comes with the memory tools that SIMD code needs: buffers aligned to 64 bytes and a buffer pool that reuses memory instead of asking the operating system again for every video frame.

## Status

| Feature | Status |
|---|---|
| x86 detection (SSE2 to AVX-512) | ✅ |
| Dispatch tables | ✅ |
| `AlignedBuffer`, `Pool` | ✅ |
| Python API | ✅ |
| ARM detection (NEON, dotprod, FP16, BF16, I8MM, SVE, SVE2) | ✅ |
| Portable vector types (`Vec128`, `Vec256`, `Vec512`) | ❌ |

## Installation

```bash
pip install aurivis-simd
```

The package contains both the Python module and the C++ headers.

## Example: what can this CPU do? (Python)

Save this as `cpu_info.py`:

```python
from aurivis_simd import Cpu, Level

class CpuReport:
    """Prints what the current machine supports."""

    @staticmethod
    def show() -> None:
        print(f'Best level:   {Cpu.best_level().name}')
        print(f'Active level: {Cpu.active_level().name}')
        print()
        for level in Level:
            print(f'  {level.name:<8} {"yes" if Cpu.supports(level) else "no"}')
        print()
        features = Cpu.features()
        print(f'AVX2 + FMA:       {features.avx2 and features.fma}')
        print(f'AVX-512 VNNI:     {features.avx512vnni}')
        print(f'ARM dot product:  {features.dot_prod}')

if __name__ == '__main__':
    CpuReport.show()
```

Run it with `python cpu_info.py`. On an Intel Core i7-11700F it prints:

```
Best level:   AVX512
Active level: AVX512

  GENERIC  yes
  SSE41    yes
  AVX2     yes
  AVX512   yes
  NEON     no
  SVE      no
  SVE2     no

AVX2 + FMA:       True
AVX-512 VNNI:     True
ARM dot product:  False
```

The detection runs once, when you import the package. After that, every call just returns the stored result, so you can call `Cpu.features()` as often as you like. In a hot loop it's still a little faster to keep the `features` object around.

`best_level()` is the highest level the machine supports. `active_level()` is the one your code should use. They're the same unless you lower it with the `AURIVIS_SIMD` environment variable (see [Testing the fallbacks](#testing-the-fallbacks)).

Everything is typed and ships with `py.typed`, so your editor knows every attribute and return type, and the package passes `mypy --strict`.

## Example: choosing the fastest implementation (C++)

This is what the library is really for. The program below sums a million floats. It contains a plain version, an AVX2 version and an AVX-512 version of the same function, and lets aurivis-simd pick the fastest one the machine can run. Save it as `sum.cpp`:

```cpp
#include "Aurivis/Simd/Simd.hpp"

#include <cstddef>
#include <cstdio>

#if AURIVIS_ARCH_X86_64
#include <immintrin.h>
#endif

using SumFn = float (*)(const float* values, size_t count);

float SumGeneric(const float* values, size_t count) {
  float total = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    total += values[i];
  }
  return total;
}

#if AURIVIS_ARCH_X86_64

AURIVIS_TARGET_AVX2 float SumAvx2(const float* values, size_t count) {
  __m256 total = _mm256_setzero_ps();
  size_t i = 0;
  for (; i + 8 <= count; i += 8) {
    total = _mm256_add_ps(total, _mm256_load_ps(values + i));
  }
  alignas(32) float lanes[8];
  _mm256_store_ps(lanes, total);
  float sum = 0.0f;
  for (const float lane : lanes) {
    sum += lane;
  }
  for (; i < count; ++i) {
    sum += values[i];
  }
  return sum;
}

AURIVIS_TARGET_AVX512 float SumAvx512(const float* values, size_t count) {
  __m512 total = _mm512_setzero_ps();
  size_t i = 0;
  for (; i + 16 <= count; i += 16) {
    total = _mm512_add_ps(total, _mm512_load_ps(values + i));
  }
  float sum = _mm512_reduce_add_ps(total);
  for (; i < count; ++i) {
    sum += values[i];
  }
  return sum;
}

static const Aurivis::Simd::DispatchTable<SumFn> SUM{.generic = &SumGeneric, .avx2 = &SumAvx2, .avx512 = &SumAvx512};

#else

static const Aurivis::Simd::DispatchTable<SumFn> SUM{.generic = &SumGeneric};

#endif

int main() {
  // 64-byte aligned, so the AVX2 and AVX-512 versions can use aligned loads.
  Aurivis::Simd::AlignedBuffer<float> values(1'000'000);
  for (size_t i = 0; i < values.GetCount(); ++i) {
    values[i] = static_cast<float>(i % 10);
  }

  const SumFn fastest = SUM.Select();
  const SumFn plain = SUM.SelectFor(Aurivis::Simd::Level::Generic);

  std::printf("Active level: %s\n", Aurivis::Simd::LevelToString(Aurivis::Simd::GetActiveLevel()));
  std::printf("Fastest sum:  %.0f\n", fastest(values.GetData(), values.GetCount()));
  std::printf("Generic sum:  %.0f\n", plain(values.GetData(), values.GetCount()));
  return 0;
}
```

The headers come with the Python package, so `Headers.get_include()` tells the compiler where to find them.

On Windows, in a **Developer PowerShell for VS 2022**:

```powershell
$include = python -c "import aurivis_simd; print(aurivis_simd.Headers.get_include())"
cl /std:c++20 /O2 /EHsc /I $include sum.cpp
.\sum.exe
```

On Linux and macOS:

```bash
g++ -std=c++20 -O2 -I"$(python -c 'import aurivis_simd; print(aurivis_simd.Headers.get_include())')" sum.cpp -o sum
./sum
```

Output:

```
Active level: avx512
Fastest sum:  4500000
Generic sum:  4500000
```

A few things worth knowing:

- Each SIMD version is marked with its `AURIVIS_TARGET_*` macro. GCC and Clang only allow AVX2 intrinsics in functions marked for AVX2, and this is how one file can contain all versions side by side. MSVC doesn't need the macros, they're simply empty there.
- You don't have to fill in every level. This table has no SSE4.1 entry, so an SSE4.1 machine falls back to `generic`, which is the only entry you must provide.
- `Select()` makes the choice once and caches it in the table. Calling it every time costs about as much as a normal function call.
- `SelectFor(Level::Generic)` returns a specific version. Here it checks that the fast version gives the same result as the plain one.
- On ARM the SIMD versions aren't compiled at all, and the program just uses `SumGeneric`. For ARM code the table has `neon`, `sve` and `sve2` entries, and SVE functions are marked with `AURIVIS_TARGET_SVE` or `AURIVIS_TARGET_SVE2`. NEON needs no marking, every 64-bit ARM CPU has it.

## Example: reusing frame buffers across threads (C++)

Video players need a fresh buffer for every frame, often 60 times a second, and usually on more than one thread. A `Pool` hands out buffers and takes them back, so memory is only allocated a few times. In this program a decoder thread fills frames and a renderer thread shows them. Save it as `frames.cpp`:

```cpp
#include "Aurivis/Simd/Simd.hpp"

#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

using Frame = Aurivis::Simd::Pool<uint8_t>::Buffer;

int main() {
  constexpr int FRAME_COUNT = 1000;
  constexpr size_t QUEUE_LIMIT = 4;

  // One 1080p YUV 4:2:0 frame per buffer. Four are allocated up front.
  Aurivis::Simd::Pool<uint8_t> pool(1920 * 1080 * 3 / 2);
  pool.Reserve(QUEUE_LIMIT);

  std::mutex mutex;
  std::condition_variable changed;
  std::deque<Frame> queue;

  // The decoder takes a buffer from the pool, fills it and hands it over.
  std::thread decoder([&] {
    for (int i = 0; i < FRAME_COUNT; ++i) {
      Frame frame = pool.Acquire();
      frame[0] = static_cast<uint8_t>(i);
      std::unique_lock<std::mutex> lock(mutex);
      changed.wait(lock, [&] { return queue.size() < QUEUE_LIMIT; });
      queue.push_back(std::move(frame));
      changed.notify_all();
    }
  });

  // The renderer shows the frame. When `frame` goes out of scope, the buffer returns to the pool.
  std::thread renderer([&] {
    for (int i = 0; i < FRAME_COUNT; ++i) {
      std::unique_lock<std::mutex> lock(mutex);
      changed.wait(lock, [&] { return !queue.empty(); });
      Frame frame = std::move(queue.front());
      queue.pop_front();
      changed.notify_all();
    }
  });

  decoder.join();
  renderer.join();

  std::printf("Frames decoded:    %d\n", FRAME_COUNT);
  std::printf("Buffers allocated: %zu\n", pool.GetIdleCount());
  return 0;
}
```

Build it the same way as `sum.cpp`. On Linux, add `-pthread`. Output:

```
Frames decoded:    1000
Buffers allocated: 5
```

A thousand frames of 3 MB each, and only five buffers were ever allocated. Every other frame reused one of them, which takes about 14 ns instead of the roughly 7 µs a new 3 MB allocation costs.

- A buffer can be returned from a different thread than the one that took it, as the renderer does here.
- Each thread works in its own part of the pool, so threads rarely wait for each other.
- `Reserve()` allocates buffers in advance, so playback never has to wait for the operating system.
- A buffer may even outlive its pool. It's then freed when it's destroyed.
- Buffers can be moved but not copied, so it's always clear which thread owns a frame. The memory isn't zeroed, because a decoder overwrites it anyway.

If you just need one aligned buffer, use `AlignedBuffer<T>` like in `sum.cpp`. It's aligned to 64 bytes and frees itself when it goes out of scope.

## Example: your own Python extension (Cython)

The headers ship inside the wheel, so your own C++ extension can use them. Here is a complete project that sums float arrays with the fastest available version:

```
myproject/
├── pyproject.toml
├── setup.py
└── myproject/
    ├── __init__.py
    ├── _native.pyx
    └── kernels.hpp
```

`pyproject.toml` lists `aurivis-simd` as a build requirement, next to setuptools and Cython:

```toml
[build-system]
requires = ["setuptools>=77", "Cython>=3.1", "aurivis-simd"]
build-backend = "setuptools.build_meta"

[project]
name = "myproject"
version = "0.1.0"
requires-python = ">=3.11"

[tool.setuptools]
packages = ["myproject"]
```

`setup.py` passes the include directory to the compiler. setuptools compiles the `.pyx` file with Cython on its own:

```python
import sys

import aurivis_simd
from setuptools import Extension, setup

class Build:
    """Builds the native extension against the aurivis-simd headers."""

    @staticmethod
    def extensions() -> list[Extension]:
        arguments = ['/std:c++20'] if sys.platform == 'win32' else ['-std=c++20']
        native = Extension(
            'myproject._native',
            ['myproject/_native.pyx'],
            include_dirs=[str(aurivis_simd.Headers.get_include()), 'myproject'],
            language='c++',
            extra_compile_args=arguments
        )
        return [native]

setup(ext_modules=Build.extensions())
```

`myproject/kernels.hpp` contains the C++ code with a plain and an AVX2 version:

```cpp
#pragma once

#include "Aurivis/Simd/Simd.hpp"

#include <cstddef>

#if AURIVIS_ARCH_X86_64
#include <immintrin.h>
#endif

namespace MyProject {

using SumFn = float (*)(const float* values, size_t count);

inline float SumGeneric(const float* values, size_t count) {
  float total = 0.0f;
  for (size_t i = 0; i < count; ++i) {
    total += values[i];
  }
  return total;
}

#if AURIVIS_ARCH_X86_64

AURIVIS_TARGET_AVX2 inline float SumAvx2(const float* values, size_t count) {
  __m256 total = _mm256_setzero_ps();
  size_t i = 0;
  for (; i + 8 <= count; i += 8) {
    total = _mm256_add_ps(total, _mm256_load_ps(values + i));
  }
  alignas(32) float lanes[8];
  _mm256_store_ps(lanes, total);
  float sum = 0.0f;
  for (const float lane : lanes) {
    sum += lane;
  }
  for (; i < count; ++i) {
    sum += values[i];
  }
  return sum;
}

inline const Aurivis::Simd::DispatchTable<SumFn> SUM{.generic = &SumGeneric, .avx2 = &SumAvx2};

#else

inline const Aurivis::Simd::DispatchTable<SumFn> SUM{.generic = &SumGeneric};

#endif

/** @brief Sums the values with the fastest implementation this machine supports. */
inline float Sum(const float* values, size_t count) { return SUM.Select()(values, count); }

}  // namespace MyProject
```

`myproject/_native.pyx` connects it to Python. It uses `cimport` to work with the C++ API directly:

```cython
from libcpp.utility cimport move

from aurivis_simd.simd cimport AlignedBuffer, GetActiveLevel, LevelToString

cdef extern from 'kernels.hpp' namespace 'MyProject' nogil:
    float Sum(const float* values, size_t count)

class Native:
    """Fast helpers written in C++."""

    @staticmethod
    def level() -> str:
        """Returns the SIMD level the C++ code uses on this machine."""
        return LevelToString(GetActiveLevel()).decode()

    @staticmethod
    def total(values) -> float:
        """Sums a buffer of float32 values, e.g. an ``array('f')``."""
        cdef const float[::1] view = values
        cdef AlignedBuffer[float] buffer
        cdef size_t i
        cdef float result
        buffer = move(AlignedBuffer[float](view.shape[0]))
        for i in range(view.shape[0]):
            buffer[i] = view[i]
        with nogil:
            result = Sum(buffer.GetData(), buffer.GetCount())
        return result
```

`myproject/__init__.py` exports it:

```python
from ._native import Native

__all__ = ['Native']
```

Install the project and try it:

```bash
pip install .
python -c "from array import array; from myproject import Native; print(Native.level(), Native.total(array('f', [i % 10 for i in range(1_000_000)])))"
```

```
avx512 4500000.0
```

Two things to note about the Cython side:

- `AlignedBuffer` and `Pool.Buffer` can only be moved, not copied, so assign them with `move(...)`.
- Errors from C++ arrive as normal Python exceptions: an invalid alignment raises `ValueError`, running out of memory raises `MemoryError`.

Since the headers are compiled into your extension, your users don't need `aurivis-simd` installed at runtime.

## Testing the fallbacks

On a fast machine you'd normally only ever run the fastest version of your code. The `AURIVIS_SIMD` environment variable lets you pretend the CPU is older, so you can test the other versions too.

PowerShell:

```powershell
$env:AURIVIS_SIMD = 'sse41'
python cpu_info.py
```

Bash:

```bash
AURIVIS_SIMD=sse41 python cpu_info.py
```

```
Best level:   AVX512
Active level: SSE41
...
```

Accepted values are `generic`, `sse41`, `avx2`, `avx512`, `neon`, `sve` and `sve2`. The variable can only lower the level, never raise it beyond what the CPU supports, and invalid values are simply ignored. It's also useful in bug reports: if a problem goes away with `AURIVIS_SIMD=generic`, it's in one of the SIMD versions.

## Levels

Each level stands for a set of instructions that all have to be present. The x86 levels match the standard x86-64 microarchitecture levels.

| Level | Required features |
|---|---|
| `Generic` | none, runs everywhere |
| `Sse41` | x86-64-v2: SSE3, SSSE3, SSE4.1, SSE4.2, POPCNT |
| `Avx2` | x86-64-v3: AVX, AVX2, FMA, BMI1, BMI2, F16C, LZCNT, MOVBE |
| `Avx512` | x86-64-v4: AVX-512 F, BW, CD, DQ, VL |
| `Neon` | ARMv8 Advanced SIMD, present on every 64-bit ARM CPU |
| `Sve` | Scalable Vector Extension, e.g. AWS Graviton 3 |
| `Sve2` | SVE2, e.g. ARMv9 chips like Graviton 4 or Microsoft Cobalt 100 |

On ARM, the levels only say whether NEON, SVE or SVE2 is there. The smaller extensions come in different combinations on different chips, so they're reported as single features in `CpuFeatures` and you check them yourself: `dot_prod` for int8 dot products, `i8mm` for int8 matrix multiplication, `neon_fp16` for half-precision math and `neon_bf16` for bfloat16. Apple chips have dot products and FP16 from the M1 on, BF16 and I8MM from the M2 on, and no SVE.

A feature only counts as available when the operating system supports it too. Some systems can disable AVX even though the CPU has it, and that case is handled for you. ARM CPUs don't let normal programs read their feature registers everywhere, so on ARM aurivis-simd asks the operating system instead: the auxiliary vector on Linux, `sysctl` on macOS and `IsProcessorFeaturePresent` on Windows. SVE is only reported when the system also saves the SVE registers. macOS 11 doesn't report the details, so there you only get NEON.

## Benchmarks

We compared aurivis-simd with [Google Highway](https://github.com/google/highway) 1.4.0 for C++, and with [NumPy](https://numpy.org) 2.5 and [py-cpuinfo](https://github.com/workhorsy/py-cpuinfo) 9.0 for Python. Everything was measured on an Intel Core i7-11700F with Windows 11, MSVC 2022 and Python 3.14, using Release builds and the median of several runs. Expect your numbers to differ a little; the code is in [`benchmarks/`](benchmarks) if you want to run it yourself.

### Detection

| | aurivis-simd | Highway |
|---|---|---|
| Asking again after the first detection | **≈ 0 ns** ¹ | 341 ns ² |
| Running the detection itself | **331 ns** | 341 ns |
| Very first call in a new process | 2.1 µs | **0.6 µs** ³ |

### Dispatch

| | aurivis-simd | Highway |
|---|---|---|
| Choosing the implementation on every call | **1.17 ns** | 1.41 ns |
| Calling a stored function pointer | **0.93 ns** | 0.94 ns |
| Plain function call, for comparison | 0.70 ns | 0.70 ns |

### Memory

| | `AlignedBuffer` | `Pool::Acquire()` | `hwy::AllocateAligned` |
|---|---|---|---|
| 2 KB | 35 ns | **13.8 ns** | 48 ns |
| 64 KB | 93 ns | **13.8 ns** | 125 ns |
| 1080p frame (3 MB) | 7.0 µs | **13.8 ns** | 7.2 µs |
| 2 KB, 8 threads at once ⁴ | 41 ns | **2.2 ns** | – |

### Python

| | aurivis-simd | NumPy | py-cpuinfo |
|---|---|---|---|
| Import and first answer, new process | **10.4 ms** | 102 ms | 1,696 ms |
| Asking again, result kept | **9.8 ns** | 20.4 ns | 1,590 ms |

¹ Too fast to measure: it takes as long as an empty loop iteration (1.6 ns).<br>
² `hwy::SupportedTargets()` runs the full detection on every call. Highway remembers its dispatch choice elsewhere.<br>
³ Highway is faster on the very first call. The difference is one-time setup in a new process: the operating system loads our code into memory, and C++ prepares the thread-safe cache on first use. The detection itself is just as fast, as the row above shows.<br>
⁴ Throughput of all threads together. Each thread takes a buffer, writes to it and gives it back, in a loop.

## Development

Build the extension in place once, then run the tests:

```bash
python setup.py build_ext --inplace
python -m unittest discover -s tests -v
```

The tests cover the Python API, the `AURIVIS_SIMD` variable and the C++ headers. On ARM they also compare the detection with what the system reports elsewhere, for example `/proc/cpuinfo` on Linux. The C++ part is compiled on the fly, so you need Cython, setuptools and a C++20 compiler.

The C++ benchmarks need CMake and download Highway automatically. The Python benchmark compares against NumPy and py-cpuinfo if they're installed:

```bash
pip install cmake numpy py-cpuinfo
cmake -S benchmarks/cpp -B benchmarks/cpp/build
cmake --build benchmarks/cpp/build --config Release
python benchmarks/cpp/run_cold_start.py
python benchmarks/python/bench_detection.py
```

## Requirements

- Python 3.11 or newer
- Windows, macOS or Linux on x86_64 or arm64
- For building against the headers: a C++20 compiler (MSVC 2022, GCC 10, Clang 12 or newer)

## Part of Aurivis

aurivis-simd is one of the native libraries behind **Aurivis**, an API library for Python with its own music and media player, built from scratch. Aurivis connects to services like YouTube, plays music and video in apps and websites, and brings sound to Discord bots through integrations for discord.py and Pycord.

aurivis-simd works on its own and doesn't depend on Aurivis.

## License

Apache-2.0, see [LICENSE](LICENSE).