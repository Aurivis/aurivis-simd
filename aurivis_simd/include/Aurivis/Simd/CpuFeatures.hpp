#pragma once

#include "Aurivis/Simd/Level.hpp"

namespace Aurivis::Simd {

/**
 * @brief SIMD features that are usable on the current machine.
 * @details A feature is only reported when both the CPU and the operating system support it. For example, avx is
 *          false if the CPU has AVX but the operating system does not save the YMM registers on context switches.
 *          Features of the other architecture family are always false.
 */
struct CpuFeatures {
  // --- x86: SSE ---
  bool sse2 = false;
  bool sse3 = false;
  bool ssse3 = false;
  bool sse41 = false;
  bool sse42 = false;
  bool popcnt = false;

  // --- x86: AVX ---
  bool avx = false;
  bool avx2 = false;
  bool fma = false;
  bool bmi1 = false;
  bool bmi2 = false;
  bool f16c = false;
  bool lzcnt = false;
  bool movbe = false;
  bool avxVnni = false;
  bool gfni = false;
  bool vaes = false;
  bool vpclmulqdq = false;

  // --- x86: AVX-512 ---
  bool avx512f = false;
  bool avx512bw = false;
  bool avx512cd = false;
  bool avx512dq = false;
  bool avx512vl = false;
  bool avx512ifma = false;
  bool avx512vbmi = false;
  bool avx512vbmi2 = false;
  bool avx512vnni = false;
  bool avx512bitalg = false;
  bool avx512vpopcntdq = false;
  bool avx512bf16 = false;
  bool avx512fp16 = false;

  // --- ARM ---
  bool neon = false;
  bool neonFp16 = false;
  bool neonBf16 = false;
  bool dotProd = false;
  bool i8mm = false;
  bool sve = false;
  bool sve2 = false;

  // --- Levels ---

  /** @brief Checks the x86-64-v2 requirements of Level::Sse41. */
  constexpr bool HasX86V2() const { return sse3 && ssse3 && sse41 && sse42 && popcnt; }

  /** @brief Checks the x86-64-v3 requirements of Level::Avx2. */
  constexpr bool HasX86V3() const { return HasX86V2() && avx && avx2 && fma && bmi1 && bmi2 && f16c && lzcnt && movbe; }

  /** @brief Checks the x86-64-v4 requirements of Level::Avx512. */
  constexpr bool HasX86V4() const { return HasX86V3() && avx512f && avx512bw && avx512cd && avx512dq && avx512vl; }

  /**
   * @brief Gets the highest level whose requirements are all met.
   * @return The matching ARM or x86 level, or Level::Generic if no level is fully supported.
   */
  constexpr Level GetBestLevel() const {
    if (neon && sve && sve2) {
      return Level::Sve2;
    }
    if (neon && sve) {
      return Level::Sve;
    }
    if (neon) {
      return Level::Neon;
    }
    if (HasX86V4()) {
      return Level::Avx512;
    }
    if (HasX86V3()) {
      return Level::Avx2;
    }
    if (HasX86V2()) {
      return Level::Sse41;
    }
    return Level::Generic;
  }

  bool operator==(const CpuFeatures&) const = default;
};

}  // namespace Aurivis::Simd