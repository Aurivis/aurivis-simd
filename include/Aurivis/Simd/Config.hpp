#pragma once

/**
 * @file Config.hpp
 * @brief Detects compiler, architecture and operating system and defines the helper macros used by all aurivis-simd headers.
 * @details Every detection macro is always defined as 0 or 1, so it can be used directly in `#if`.
 */

// --- Version ---

#define AURIVIS_SIMD_VERSION_MAJOR 0
#define AURIVIS_SIMD_VERSION_MINOR 1
#define AURIVIS_SIMD_VERSION_PATCH 0

// --- Language Standard ---

#if defined(_MSVC_LANG)
#define AURIVIS_CPLUSPLUS _MSVC_LANG
#else
#define AURIVIS_CPLUSPLUS __cplusplus
#endif

#if AURIVIS_CPLUSPLUS < 202002L
#error "aurivis-simd requires C++20 or newer."
#endif

// --- Compiler ---

#if defined(__clang__)
#define AURIVIS_COMPILER_CLANG 1
#elif defined(__GNUC__)
#define AURIVIS_COMPILER_GCC 1
#elif defined(_MSC_VER)
#define AURIVIS_COMPILER_MSVC 1
#else
#error "aurivis-simd requires MSVC, GCC or Clang."
#endif

#ifndef AURIVIS_COMPILER_CLANG
#define AURIVIS_COMPILER_CLANG 0
#endif
#ifndef AURIVIS_COMPILER_GCC
#define AURIVIS_COMPILER_GCC 0
#endif
#ifndef AURIVIS_COMPILER_MSVC
#define AURIVIS_COMPILER_MSVC 0
#endif

// --- Architecture ---

// ARM64EC also defines _M_X64, so ARM has to be checked first.
#if defined(_M_ARM64EC) || defined(_M_ARM64) || defined(__aarch64__)
#define AURIVIS_ARCH_ARM64 1
#elif defined(_M_X64) || defined(__x86_64__)
#define AURIVIS_ARCH_X86_64 1
#endif

#ifndef AURIVIS_ARCH_ARM64
#define AURIVIS_ARCH_ARM64 0
#endif
#ifndef AURIVIS_ARCH_X86_64
#define AURIVIS_ARCH_X86_64 0
#endif

// --- Operating System ---

#if defined(_WIN32)
#define AURIVIS_OS_WINDOWS 1
#elif defined(__APPLE__)
#define AURIVIS_OS_APPLE 1
#elif defined(__linux__)
#define AURIVIS_OS_LINUX 1
#endif

#ifndef AURIVIS_OS_WINDOWS
#define AURIVIS_OS_WINDOWS 0
#endif
#ifndef AURIVIS_OS_APPLE
#define AURIVIS_OS_APPLE 0
#endif
#ifndef AURIVIS_OS_LINUX
#define AURIVIS_OS_LINUX 0
#endif

// --- Function Attributes ---

#if AURIVIS_COMPILER_MSVC
/** @brief Forces the compiler to inline a function. */
#define AURIVIS_FORCE_INLINE __forceinline
/** @brief Prevents the compiler from inlining a function. */
#define AURIVIS_NOINLINE __declspec(noinline)
#else
#define AURIVIS_FORCE_INLINE inline __attribute__((always_inline))
#define AURIVIS_NOINLINE __attribute__((noinline))
#endif

/** @brief Promises the compiler that a pointer does not alias any other pointer in the same scope. */
#define AURIVIS_RESTRICT __restrict

// --- SIMD Target Attributes ---

/**
 * @brief Marks a function that may use the instructions of a specific SIMD level.
 * @details GCC and Clang (including clang-cl) only allow intrinsics of a level inside functions that carry the matching
 *          target attribute, unless the whole file is compiled for that level. MSVC allows all intrinsics everywhere,
 *          so the macros are empty there. The feature lists match the x86-64-v2, v3 and v4 levels.
 */
#if AURIVIS_ARCH_X86_64 && !AURIVIS_COMPILER_MSVC
#define AURIVIS_TARGET_SSE41 __attribute__((target("sse3,ssse3,sse4.1,sse4.2,popcnt")))
#define AURIVIS_TARGET_AVX2 __attribute__((target("sse3,ssse3,sse4.1,sse4.2,popcnt,avx,avx2,fma,bmi,bmi2,f16c,lzcnt,movbe")))
#define AURIVIS_TARGET_AVX512 __attribute__((target("sse3,ssse3,sse4.1,sse4.2,popcnt,avx,avx2,fma,bmi,bmi2,f16c,lzcnt,movbe,avx512f,avx512bw,avx512cd,avx512dq,avx512vl")))
#else
#define AURIVIS_TARGET_SSE41
#define AURIVIS_TARGET_AVX2
#define AURIVIS_TARGET_AVX512
#endif

// NEON is part of every ARMv8 CPU, so no attribute is needed.
#define AURIVIS_TARGET_NEON