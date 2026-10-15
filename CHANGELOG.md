# Changelog

All notable changes to this project are documented here. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Until 1.0, minor versions may still contain breaking changes.

## [Unreleased]

### Added

- **ARM detection** of dotprod, FP16, BF16, I8MM, SVE and SVE2 on Linux, macOS and Windows. Like on x86, a feature only counts as available when the operating system supports it too.
- **Levels** `Sve` and `Sve2`, with matching `sve` and `sve2` entries in `DispatchTable`, `Level.SVE` and `Level.SVE2` in Python, and `sve` and `sve2` as values for `AURIVIS_SIMD`.
- **`AURIVIS_TARGET_SVE` and `AURIVIS_TARGET_SVE2`** for marking SVE functions with GCC and Clang.

### Changed

- On ARM machines with SVE, `GetBestLevel()` and `Cpu.best_level()` now return `Sve` or `Sve2` instead of `Neon`. To check for NEON, use `IsSupportedBy()` or `Cpu.supports(Level.NEON)` instead of comparing levels.

## [0.1.0] - 2026-10-15

The first release. It covers everything needed for x86 SIMD code: finding out what the CPU can do, picking the fastest implementation and managing aligned memory.

### Added

- **CPU detection** for x86-64 from SSE2 to AVX-512, including VNNI, VBMI2, BF16, FP16, GFNI and VAES. A feature only counts as available when the operating system supports it too.
- **Levels** `Generic`, `Sse41`, `Avx2`, `Avx512` and `Neon`, matching the x86-64-v2, v3 and v4 microarchitecture levels.
- **`DispatchTable`**, which picks the fastest implementation of a function, falls back to lower levels when an entry is missing and caches its choice after the first call.
- **`AlignedBuffer`**, a move-only buffer aligned to 64 bytes by default.
- **`Pool`**, a thread-safe buffer pool with per-thread shards. Buffers can be returned from any thread and may outlive their pool.
- **`AURIVIS_SIMD`** environment variable to lower the active level for testing and bug reports.
- **Python API** with `Cpu`, `CpuFeatures`, `Level`, `Headers` and `IncludePath`. It is fully typed, ships `py.typed` and passes `mypy --strict`.
- **Cython declarations** in `aurivis_simd.simd` for using the C++ API with `cimport`.
- **Headers in the wheel**, so other extensions can build against them through `Headers.get_include()`.
- **Wheels** for CPython 3.11 to 3.14 on Windows, macOS and Linux, each for x86_64 and ARM64, including musllinux.
- **Tests** for the Python API, the environment variable and the C++ headers, run on every supported platform.
- **Benchmarks** against Google Highway, NumPy and py-cpuinfo.

### Known limitations

- On ARM, only NEON is detected. Finer detection of dotprod, SVE and SVE2 will follow.
- Portable vector types (`Vec128`, `Vec256`, `Vec512`) are not available yet.

[Unreleased]: https://github.com/Aurivis/aurivis-simd/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/Aurivis/aurivis-simd/releases/tag/v0.1.0