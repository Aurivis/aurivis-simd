#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Aurivis::Simd {

/**
 * @brief SIMD instruction set levels, ordered within each architecture family so that every level includes the ones below it.
 * @details The x86 levels follow the x86-64 microarchitecture levels v2, v3 and v4. On ARM, every AArch64 CPU has NEON,
 *          and SVE2 always comes with SVE. The numeric values separate the families: 1 to 15 are x86, 16 to 31 are
 *          ARM. Levels of different families must not be compared with < or >. Use IsSupportedBy() instead.
 */
enum class Level : uint8_t {
  Generic = 0,  // Plain C++, runs everywhere
  Sse41 = 1,    // x86-64-v2
  Avx2 = 2,     // x86-64-v3
  Avx512 = 3,   // x86-64-v4
  Neon = 16,    // ARMv8 Advanced SIMD
  Sve = 17,     // Scalable Vector Extension
  Sve2 = 18     // Scalable Vector Extension 2 (ARMv9)
};

/** @brief All levels in ascending order. */
inline constexpr std::array<Level, 7> ALL_LEVELS = {Level::Generic, Level::Sse41, Level::Avx2, Level::Avx512, Level::Neon, Level::Sve, Level::Sve2};

namespace Detail {

inline constexpr uint8_t ARM_FAMILY_START = 16;

constexpr bool IsArmFamily(Level level) { return static_cast<uint8_t>(level) >= ARM_FAMILY_START; }

constexpr char ToLowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

constexpr bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t i = 0; i < left.size(); ++i) {
    if (ToLowerAscii(left[i]) != ToLowerAscii(right[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace Detail

/**
 * @brief Checks whether code built for one level can run when another level is active.
 * @param candidate The level the code was built for.
 * @param active The level the CPU allows, possibly lowered by AURIVIS_SIMD.
 * @return True if candidate is Generic, or belongs to the same family as active and is not above it.
 */
constexpr bool IsSupportedBy(Level candidate, Level active) {
  if (candidate == Level::Generic) {
    return true;
  }
  if (Detail::IsArmFamily(candidate) != Detail::IsArmFamily(active)) {
    return false;
  }
  return static_cast<uint8_t>(candidate) <= static_cast<uint8_t>(active);
}

/**
 * @brief Converts a level to its lowercase name.
 * @param level The level to convert.
 * @return The name as accepted by AURIVIS_SIMD (e.g. "avx2"), or "unknown" for invalid values.
 */
constexpr const char* LevelToString(Level level) {
  switch (level) {
    case Level::Generic:
      return "generic";
    case Level::Sse41:
      return "sse41";
    case Level::Avx2:
      return "avx2";
    case Level::Avx512:
      return "avx512";
    case Level::Neon:
      return "neon";
    case Level::Sve:
      return "sve";
    case Level::Sve2:
      return "sve2";
    default:
      return "unknown";
  }
}

/**
 * @brief Attempts to parse a level name.
 * @param text The name to parse (case-insensitive, e.g. "AVX2" or "avx2").
 * @param outLevel Receives the parsed level on success and is left unchanged otherwise.
 * @return True if the name matched a level, false otherwise.
 */
constexpr bool TryParseLevel(std::string_view text, Level& outLevel) {
  for (const Level level : ALL_LEVELS) {
    if (Detail::EqualsIgnoreCase(text, LevelToString(level))) {
      outLevel = level;
      return true;
    }
  }
  return false;
}

}  // namespace Aurivis::Simd