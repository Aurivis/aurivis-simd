#pragma once

#include "Aurivis/Simd/Cpu.hpp"
#include "Aurivis/Simd/Level.hpp"

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace Aurivis::Simd {

/**
 * @brief A set of implementations of one function, one per SIMD level.
 * @details Fill the members with designated initializers. generic is required, all others are optional and may stay
 *          nullptr. Select() picks the best implementation the active level allows. In hot loops, call Select() once
 *          and keep the returned function pointer.
 *
 * @tparam Fn Function pointer type shared by all implementations.
 */
template <typename Fn>
struct DispatchTable {
  static_assert(std::is_pointer_v<Fn> && std::is_function_v<std::remove_pointer_t<Fn>>, "DispatchTable expects a function pointer type.");

  Fn generic = nullptr;
  Fn sse41 = nullptr;
  Fn avx2 = nullptr;
  Fn avx512 = nullptr;
  Fn neon = nullptr;

  // Internal cache filled by Select(). Leave it out of the initializer.
  mutable std::atomic<Fn> cachedSelection{nullptr};

  /**
   * @brief Selects the best implementation for the active level.
   * @details The first call resolves the choice, later calls only read the cached function pointer. The active level
   *          never changes after it was resolved, so the cache never goes stale. This method is thread-safe.
   * @return The best non-null implementation not above GetActiveLevel(), or generic.
   */
  Fn Select() const noexcept {
    Fn selected = cachedSelection.load(std::memory_order_relaxed);
    if (selected != nullptr) [[likely]] {
      return selected;
    }
    selected = SelectFor(GetActiveLevel());
    cachedSelection.store(selected, std::memory_order_relaxed);
    return selected;
  }

  /**
   * @brief Selects the best implementation that a given level allows.
   * @param level The level to select for, e.g. Level::Generic to force the fallback in tests.
   * @return The best non-null implementation not above level, or generic.
   */
  constexpr Fn SelectFor(Level level) const noexcept {
    if (level == Level::Neon) {
      return neon != nullptr ? neon : generic;
    }
    // Only x86 levels remain, and their values are ordered: Generic 0, Sse41 1, Avx2 2, Avx512 3.
    const auto rank = static_cast<uint8_t>(level);
    if (rank >= static_cast<uint8_t>(Level::Avx512) && avx512 != nullptr) {
      return avx512;
    }
    if (rank >= static_cast<uint8_t>(Level::Avx2) && avx2 != nullptr) {
      return avx2;
    }
    if (rank >= static_cast<uint8_t>(Level::Sse41) && sse41 != nullptr) {
      return sse41;
    }
    return generic;
  }
};

}  // namespace Aurivis::Simd