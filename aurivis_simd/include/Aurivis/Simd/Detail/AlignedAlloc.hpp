#pragma once

#include "Aurivis/Simd/Config.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#if AURIVIS_OS_WINDOWS
#include <malloc.h>
#else
#include <stdlib.h>
#endif

namespace Aurivis::Simd::Detail {

/** @brief Checks whether a value is a power of two. Zero is not a power of two. */
constexpr bool IsPowerOfTwo(size_t value) noexcept { return value != 0 && (value & (value - 1)) == 0; }

/**
 * @brief Checks whether an alignment can be passed to AllocateAligned().
 * @details The platform allocators require a power of two that is at least the size of a pointer.
 */
constexpr bool IsValidAlignment(size_t alignment) noexcept { return IsPowerOfTwo(alignment) && alignment >= sizeof(void*); }

/** @brief Checks whether a pointer starts at a multiple of the given alignment. */
inline bool IsAligned(const void* pointer, size_t alignment) noexcept { return (reinterpret_cast<uintptr_t>(pointer) & (alignment - 1)) == 0; }

/**
 * @brief Allocates memory whose start address is a multiple of the given alignment.
 * @param size Number of bytes to allocate.
 * @param alignment Required alignment in bytes, see IsValidAlignment().
 * @return The allocated memory, or nullptr if size is 0, the alignment is invalid or the allocation failed.
 *         The memory must be released with FreeAligned().
 */
inline void* AllocateAligned(size_t size, size_t alignment) noexcept {
  if (size == 0 || !IsValidAlignment(alignment)) {
    return nullptr;
  }
#if AURIVIS_OS_WINDOWS
  return _aligned_malloc(size, alignment);
#else
  void* pointer = nullptr;
  if (posix_memalign(&pointer, alignment, size) != 0) {
    return nullptr;
  }
  return pointer;
#endif
}

/**
 * @brief Releases memory returned by AllocateAligned().
 * @param pointer The memory to release. Passing nullptr is allowed and does nothing.
 */
inline void FreeAligned(void* pointer) noexcept {
#if AURIVIS_OS_WINDOWS
  _aligned_free(pointer);
#else
  std::free(pointer);
#endif
}

}  // namespace Aurivis::Simd::Detail