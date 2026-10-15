#pragma once

#include "Aurivis/Simd/Config.hpp"
#include "Aurivis/Simd/CpuFeatures.hpp"
#include "Aurivis/Simd/Detail/CpuArm.hpp"
#include "Aurivis/Simd/Detail/CpuX86.hpp"
#include "Aurivis/Simd/Level.hpp"

#include <atomic>
#include <cstdlib>
#include <string_view>

#if AURIVIS_OS_WINDOWS
// Declared by hand to avoid including <windows.h>. Identical to the declaration in <processenv.h>.
extern "C" __declspec(dllimport) unsigned long __stdcall GetEnvironmentVariableW(const wchar_t* name, wchar_t* buffer, unsigned long size);
#endif

namespace Aurivis::Simd {

namespace Detail {

/** @brief Name of the environment variable that limits the active level. */
inline constexpr const char* LEVEL_LIMIT_VARIABLE = "AURIVIS_SIMD";

/** @brief Longest accepted value of the environment variable, in characters. */
inline constexpr unsigned long LEVEL_LIMIT_MAX_LENGTH = 15;

/** @brief The hardware detection result, detected once and then cached. */
struct DetectionState {
  CpuFeatures features;
  Level bestLevel = Level::Generic;
};

// --- Cached State ---
inline DetectionState s_detectionState;
inline std::atomic<bool> s_isDetected{false};
inline Level s_activeLevel = Level::Generic;
inline std::atomic<bool> s_isActiveLevelResolved{false};

/** @brief Runs the platform-specific detection without caching. */
inline CpuFeatures DetectFeatures() noexcept {
#if AURIVIS_ARCH_X86_64
  return DetectX86();
#elif AURIVIS_ARCH_ARM64
  return DetectArm();
#else
  return CpuFeatures{};
#endif
}

/**
 * @brief Attempts to read a level from the AURIVIS_SIMD environment variable.
 * @param outLevel Receives the level if the variable is set to a valid level name.
 * @return True if a valid level name was read, false otherwise.
 */
inline bool TryReadLevelLimit(Level& outLevel) noexcept {
#if AURIVIS_OS_WINDOWS
  // Reading the wide environment block directly is much cheaper than the CRT, which converts the whole
  // environment to narrow strings on its first access.
  wchar_t wide[LEVEL_LIMIT_MAX_LENGTH + 1] = {};
  const unsigned long length = GetEnvironmentVariableW(L"AURIVIS_SIMD", wide, LEVEL_LIMIT_MAX_LENGTH + 1);
  if (length == 0 || length > LEVEL_LIMIT_MAX_LENGTH) {
    return false;  // Not set, or too long to be a level name
  }
  char narrow[LEVEL_LIMIT_MAX_LENGTH + 1] = {};
  for (unsigned long i = 0; i < length; ++i) {
    if (wide[i] > 0x7F) {
      return false;
    }
    narrow[i] = static_cast<char>(wide[i]);
  }
  return TryParseLevel(std::string_view(narrow, length), outLevel);
#else
  const char* value = std::getenv(LEVEL_LIMIT_VARIABLE);
  return value != nullptr && TryParseLevel(value, outLevel);
#endif
}

/**
 * @brief Applies the AURIVIS_SIMD limit to the detected level.
 * @param bestLevel The highest level the machine supports.
 * @return The requested level if it is valid and supported, otherwise bestLevel.
 */
inline Level ApplyLevelLimit(Level bestLevel) noexcept {
  Level requested = bestLevel;
  if (TryReadLevelLimit(requested) && IsSupportedBy(requested, bestLevel)) {
    return requested;
  }
  return bestLevel;
}

/**
 * @brief Detects the hardware features once and publishes the result.
 * @details The function-local static guarantees that only one thread runs the detection. Kept out of line so the
 *          fast path in GetDetectionState() stays small.
 */
AURIVIS_NOINLINE inline const DetectionState& InitializeDetectionState() noexcept {
  static const bool isInitialized = [] {
    DetectionState state;
    state.features = DetectFeatures();
    state.bestLevel = state.features.GetBestLevel();
    s_detectionState = state;
    s_isDetected.store(true, std::memory_order_release);
    return true;
  }();
  static_cast<void>(isInitialized);
  return s_detectionState;
}

/**
 * @brief Gets the cached detection result, detecting on the first call.
 * @details After the first call this is a single atomic load and a branch. This function is thread-safe.
 */
inline const DetectionState& GetDetectionState() noexcept {
  if (s_isDetected.load(std::memory_order_acquire)) [[likely]] {
    return s_detectionState;
  }
  return InitializeDetectionState();
}

/**
 * @brief Resolves the active level once and publishes the result.
 * @details Separate from the hardware detection, so Detect() never pays for reading the environment.
 */
AURIVIS_NOINLINE inline Level InitializeActiveLevel() noexcept {
  static const bool isInitialized = [] {
    s_activeLevel = ApplyLevelLimit(GetDetectionState().bestLevel);
    s_isActiveLevelResolved.store(true, std::memory_order_release);
    return true;
  }();
  static_cast<void>(isInitialized);
  return s_activeLevel;
}

}  // namespace Detail

/**
 * @brief Gets the SIMD features of the current machine.
 * @details Detection runs once on the first call. Later calls return the cached result. This function is thread-safe.
 * @return The features that both the CPU and the operating system support.
 */
inline const CpuFeatures& Detect() noexcept { return Detail::GetDetectionState().features; }

/**
 * @brief Gets the highest level the machine supports, ignoring AURIVIS_SIMD.
 * @details This function is thread-safe.
 */
inline Level GetBestLevel() noexcept { return Detail::GetDetectionState().bestLevel; }

/**
 * @brief Gets the level that dispatch should use.
 * @details Equal to GetBestLevel() unless AURIVIS_SIMD names a lower supported level. The variable is read once,
 *          on the first call, so Detect() never pays for it. This function is thread-safe.
 */
inline Level GetActiveLevel() noexcept {
  if (Detail::s_isActiveLevelResolved.load(std::memory_order_acquire)) [[likely]] {
    return Detail::s_activeLevel;
  }
  return Detail::InitializeActiveLevel();
}

}  // namespace Aurivis::Simd