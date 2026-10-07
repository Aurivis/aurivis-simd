#pragma once

#include "Aurivis/Simd/Config.hpp"
#include "Aurivis/Simd/CpuFeatures.hpp"

#include <cstdint>

#if AURIVIS_ARCH_X86_64

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif

namespace Aurivis::Simd::Detail {

/** @brief The four registers returned by the CPUID instruction. */
struct CpuidRegisters {
  uint32_t eax = 0;
  uint32_t ebx = 0;
  uint32_t ecx = 0;
  uint32_t edx = 0;
};

/** @brief XCR0 bits that must be set for XMM and YMM registers (SSE and AVX). */
inline constexpr uint64_t XCR0_YMM_STATE = 0x06;

/** @brief XCR0 bits that must be set for XMM, YMM, opmask and ZMM registers (AVX-512). */
inline constexpr uint64_t XCR0_ZMM_STATE = 0xE6;

constexpr bool IsBitSet(uint32_t value, uint32_t bit) noexcept { return ((value >> bit) & 1u) != 0; }

/**
 * @brief Executes the CPUID instruction.
 * @param leaf The main function number (EAX).
 * @param subleaf The sub-function number (ECX).
 * @return The values of EAX, EBX, ECX and EDX.
 */
inline CpuidRegisters Cpuid(uint32_t leaf, uint32_t subleaf = 0) noexcept {
#if defined(_MSC_VER)
  int registers[4] = {};
  __cpuidex(registers, static_cast<int>(leaf), static_cast<int>(subleaf));
  return {static_cast<uint32_t>(registers[0]), static_cast<uint32_t>(registers[1]), static_cast<uint32_t>(registers[2]), static_cast<uint32_t>(registers[3])};
#else
  CpuidRegisters registers;
  __cpuid_count(leaf, subleaf, registers.eax, registers.ebx, registers.ecx, registers.edx);
  return registers;
#endif
}

/**
 * @brief Reads XCR0, which tells which register states the operating system saves on context switches.
 * @details Must only be called when CPUID reports OSXSAVE.
 */
inline uint64_t ReadXcr0() noexcept {
#if AURIVIS_COMPILER_MSVC
  return _xgetbv(0);
#else
  // Inline assembly avoids the "xsave" target attribute that GCC and Clang require for _xgetbv().
  uint32_t eax = 0;
  uint32_t edx = 0;
  __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
  return (static_cast<uint64_t>(edx) << 32) | eax;
#endif
}

/**
 * @brief Detects the x86 SIMD features that both the CPU and the operating system support.
 * @return The detected features. ARM fields stay false.
 */
inline CpuFeatures DetectX86() noexcept {
  CpuFeatures features;

  const uint32_t maxLeaf = Cpuid(0).eax;
  if (maxLeaf < 1) {
    return features;
  }

  // --- Leaf 1: SSE family, MOVBE, POPCNT ---
  const CpuidRegisters leaf1 = Cpuid(1);
  features.sse2 = IsBitSet(leaf1.edx, 26);
  features.sse3 = IsBitSet(leaf1.ecx, 0);
  features.ssse3 = IsBitSet(leaf1.ecx, 9);
  features.sse41 = IsBitSet(leaf1.ecx, 19);
  features.sse42 = IsBitSet(leaf1.ecx, 20);
  features.movbe = IsBitSet(leaf1.ecx, 22);
  features.popcnt = IsBitSet(leaf1.ecx, 23);

  // --- Operating system support for YMM and ZMM registers ---
  const bool hasOsXsave = IsBitSet(leaf1.ecx, 27);
  const uint64_t xcr0 = hasOsXsave ? ReadXcr0() : 0;
  const bool ymmEnabled = (xcr0 & XCR0_YMM_STATE) == XCR0_YMM_STATE;
#if AURIVIS_OS_APPLE
  // macOS enables the AVX-512 state lazily on first use, so XCR0 does not report it in advance.
  const bool zmmEnabled = ymmEnabled;
#else
  const bool zmmEnabled = (xcr0 & XCR0_ZMM_STATE) == XCR0_ZMM_STATE;
#endif

  // --- Leaf 1: AVX family (needs YMM state) ---
  features.avx = ymmEnabled && IsBitSet(leaf1.ecx, 28);
  features.fma = ymmEnabled && IsBitSet(leaf1.ecx, 12);
  features.f16c = ymmEnabled && IsBitSet(leaf1.ecx, 29);

  if (maxLeaf >= 7) {
    // --- Leaf 7, subleaf 0: BMI, GFNI, AVX2, AVX-512 ---
    const CpuidRegisters leaf7 = Cpuid(7, 0);
    features.bmi1 = IsBitSet(leaf7.ebx, 3);
    features.bmi2 = IsBitSet(leaf7.ebx, 8);
    features.gfni = IsBitSet(leaf7.ecx, 8);

    features.avx2 = ymmEnabled && IsBitSet(leaf7.ebx, 5);
    features.vaes = ymmEnabled && IsBitSet(leaf7.ecx, 9);
    features.vpclmulqdq = ymmEnabled && IsBitSet(leaf7.ecx, 10);

    features.avx512f = zmmEnabled && IsBitSet(leaf7.ebx, 16);
    features.avx512dq = zmmEnabled && IsBitSet(leaf7.ebx, 17);
    features.avx512ifma = zmmEnabled && IsBitSet(leaf7.ebx, 21);
    features.avx512cd = zmmEnabled && IsBitSet(leaf7.ebx, 28);
    features.avx512bw = zmmEnabled && IsBitSet(leaf7.ebx, 30);
    features.avx512vl = zmmEnabled && IsBitSet(leaf7.ebx, 31);
    features.avx512vbmi = zmmEnabled && IsBitSet(leaf7.ecx, 1);
    features.avx512vbmi2 = zmmEnabled && IsBitSet(leaf7.ecx, 6);
    features.avx512vnni = zmmEnabled && IsBitSet(leaf7.ecx, 11);
    features.avx512bitalg = zmmEnabled && IsBitSet(leaf7.ecx, 12);
    features.avx512vpopcntdq = zmmEnabled && IsBitSet(leaf7.ecx, 14);
    features.avx512fp16 = zmmEnabled && IsBitSet(leaf7.edx, 23);

    // --- Leaf 7, subleaf 1: AVX-VNNI, AVX-512 BF16 ---
    if (leaf7.eax >= 1) {
      const CpuidRegisters leaf7Sub1 = Cpuid(7, 1);
      features.avxVnni = ymmEnabled && IsBitSet(leaf7Sub1.eax, 4);
      features.avx512bf16 = zmmEnabled && IsBitSet(leaf7Sub1.eax, 5);
    }
  }

  // --- Extended leaf 0x80000001: LZCNT ---
  if (Cpuid(0x80000000).eax >= 0x80000001) {
    features.lzcnt = IsBitSet(Cpuid(0x80000001).ecx, 5);
  }

  return features;
}

}  // namespace Aurivis::Simd::Detail

#endif  // AURIVIS_ARCH_X86_64