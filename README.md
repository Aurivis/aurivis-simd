<div align="center">

# aurivis-simd

**CPU feature detection, runtime dispatch and aligned memory for SIMD code.**<br>
Header-only C++20 with a small, fully typed Python API.

<br>

![Python](https://img.shields.io/badge/Python-3.11%2B-3776AB?style=for-the-badge&logo=python&logoColor=white&labelColor=1F2937)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white&labelColor=1F2937)
![Platforms](https://img.shields.io/badge/Platforms-Windows%20%7C%20macOS%20%7C%20Linux-6E40C9?style=for-the-badge&labelColor=1F2937)
[![License](https://img.shields.io/badge/License-Apache%202.0-D22128?style=for-the-badge&logo=apache&logoColor=white&labelColor=1F2937)](LICENSE)

</div>

<br>

## Why this exists

Modern CPUs can process many values with a single instruction. These are the SIMD instruction sets: SSE, AVX2, AVX-512 on Intel and AMD, NEON on ARM. Used well, they make code like video decoding, audio processing or parsing several times faster.

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
| ARM detection (NEON, dotprod, SVE, SVE2) | ❌ |
| Portable vector types (`Vec128`, `Vec256`, `Vec512`) | ❌ |

## Installation

```bash
pip install aurivis-simd
```

If you only want to build against the C++ headers, see [Building your own extension](#building-your-own-extension).

## Using it from Python

```python
from aurivis_simd import Cpu, Level

Cpu.best_level()
# Level.AVX512

Cpu.supports(Level.AVX2)
# True

features = Cpu.features()
features.avx2
# True
```

The detection runs once, when you import the package. After that, every call just returns the stored result. If you check features inside a hot loop, keep the `features` object around instead of calling `Cpu.features()` each time.

`Cpu.active_level()` is the level your code should actually use. Normally it's the same as `best_level()`, but you can lower it with the `AURIVIS_SIMD` environment variable, which is handy for testing (see [Testing the fallbacks](#testing-the-fallbacks)).

Everything is typed and ships with `py.typed`, so your editor knows every attribute and return type, and the package passes `mypy --strict`.

## Using it from C++

Include the umbrella header and you have everything:

```cpp
#include "Aurivis/Simd/Simd.hpp"
```

### Finding out what the CPU can do

```cpp
const Aurivis::Simd::CpuFeatures& features = Aurivis::Simd::Detect();

if (features.avx2) {
    // ...
}
```

`Detect()` checks the CPU on the first call and remembers the result, so you can call it as often as you like. A feature only counts as available when the operating system supports it too. Some systems can disable AVX even though the CPU has it, and that case is handled for you.

### Picking the fastest implementation

This is what the library is really for. Write your function once per level, put the versions into a `DispatchTable` and let it choose:

```cpp
using TransformFn = void (*)(const int16_t* input, uint8_t* output, size_t stride);

void TransformGeneric(const int16_t* input, uint8_t* output, size_t stride);
AURIVIS_TARGET_SSE41 void TransformSse41(const int16_t* input, uint8_t* output, size_t stride);
AURIVIS_TARGET_AVX2 void TransformAvx2(const int16_t* input, uint8_t* output, size_t stride);

static const Aurivis::Simd::DispatchTable<TransformFn> transform{
    .generic = &TransformGeneric,
    .sse41 = &TransformSse41,
    .avx2 = &TransformAvx2,
};

transform.Select()(input, output, stride);
```

You don't have to fill in every level. If there's no AVX2 version, the table falls back to SSE4.1, and in the end to `generic`, which is the only one you must provide. The choice is made once and then cached in the table, so calling `Select()` every time costs about as much as a regular function call.

The `AURIVIS_TARGET_*` macros tell GCC and Clang that a function may use the instructions of that level. That way, one source file can contain the plain version and the AVX2 version side by side.

To test a specific version, for example to check that the AVX2 version gives the same result as the plain one, use `SelectFor()`:

```cpp
const TransformFn plain = transform.SelectFor(Aurivis::Simd::Level::Generic);
```

### Aligned memory

SIMD instructions are fastest when data starts at an aligned address. `AlignedBuffer` takes care of that:

```cpp
Aurivis::Simd::AlignedBuffer<int16_t> coefficients(32 * 32);
int16_t* data = coefficients.GetData();
```

Buffers are aligned to 64 bytes by default and are freed automatically. They can be moved but not copied, so it's always clear who owns the memory. The memory isn't zeroed, because SIMD code usually overwrites it right away anyway.

### Reusing buffers with a pool

If you need the same kind of buffer over and over, like one per video frame, a `Pool` is much faster than allocating each time:

```cpp
Aurivis::Simd::Pool<uint8_t> frames(1920 * 1080 * 3 / 2);
frames.Reserve(8);

auto frame = frames.Acquire();
```

When `frame` goes out of scope, the buffer goes back into the pool and the next `Acquire()` hands it out again. `Reserve()` allocates a few buffers up front, so playback never has to wait for the operating system.

The pool is built for multiple threads. Each thread works in its own part of the pool, so they rarely get in each other's way, and a buffer can be returned from a different thread than the one that took it. That's exactly what happens when a decoder thread produces frames and a render thread displays them. A buffer may even outlive its pool; it's freed when it's destroyed.

## Building your own extension

The headers ship inside the wheel, so another Python extension can build against them. Add `aurivis-simd` to your build requirements:

```toml
[build-system]
requires = ["setuptools", "Cython", "aurivis-simd"]
```

Then pass the include directory to your extension. setuptools compiles `.pyx` files with Cython on its own:

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

Use `-std=c++20` instead of `/std:c++20` with GCC and Clang. Since the headers are compiled into your extension, your users don't need `aurivis-simd` installed at runtime.

## Cython

You can use the C++ API directly from Cython with `cimport`:

```cython
# distutils: language = c++

from libcpp.utility cimport move

from aurivis_simd.simd cimport AlignedBuffer, GetActiveLevel, Level

if GetActiveLevel() == Level.Avx2:
    ...

cdef AlignedBuffer[float] buffer
buffer = move(AlignedBuffer[float](1024))
```

`AlignedBuffer` and `Pool.Buffer` can only be moved, so assign them with `move(...)`. Errors from C++ arrive as normal Python exceptions: an invalid alignment raises `ValueError`, running out of memory raises `MemoryError`.

## Testing the fallbacks

On a fast machine, you'd normally only ever run the fastest version of your code. The `AURIVIS_SIMD` environment variable lets you pretend the CPU is older:

```bash
AURIVIS_SIMD=generic python app.py
```

Accepted values are `generic`, `sse41`, `avx2`, `avx512` and `neon`. The variable can only lower the level, never raise it beyond what the CPU supports, and invalid values are simply ignored. It's also useful in bug reports: if a problem goes away with `AURIVIS_SIMD=generic`, it's in one of the SIMD versions.

## Levels

Each level stands for a set of instructions that all have to be present. The x86 levels match the standard x86-64 microarchitecture levels.

| Level | Required features |
|---|---|
| `Generic` | none, runs everywhere |
| `Sse41` | x86-64-v2: SSE3, SSSE3, SSE4.1, SSE4.2, POPCNT |
| `Avx2` | x86-64-v3: AVX, AVX2, FMA, BMI1, BMI2, F16C, LZCNT, MOVBE |
| `Avx512` | x86-64-v4: AVX-512 F, BW, CD, DQ, VL |
| `Neon` | ARMv8 Advanced SIMD |

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

The tests cover the Python API, the `AURIVIS_SIMD` variable and the C++ headers. The C++ part is compiled on the fly, so you need Cython, setuptools and a C++20 compiler.

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