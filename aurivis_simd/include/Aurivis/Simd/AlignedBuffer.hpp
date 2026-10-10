#pragma once

#include "Aurivis/Simd/Detail/AlignedAlloc.hpp"

#include <cstddef>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace Aurivis::Simd {

/** @brief Default alignment in bytes: one cache line, enough for AVX-512 loads and stores. */
inline constexpr size_t DEFAULT_ALIGNMENT = 64;

/**
 * @class AlignedBuffer
 * @brief A heap buffer whose first element is aligned for SIMD loads and stores.
 * @details The memory is not initialized, like `new T[count]` for integers and floats. It is released automatically
 *          when the buffer is destroyed. The buffer can be moved but not copied.
 *
 * @tparam T Element type. Must be trivial, e.g. an integer or floating-point type.
 */
template <typename T>
class AlignedBuffer {
  static_assert(std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>, "AlignedBuffer only holds trivial types such as integers and floats.");

 public:
  /** @brief Creates an empty buffer without allocating. */
  AlignedBuffer() noexcept = default;

  /**
   * @brief Allocates uninitialized memory for count elements.
   * @param count Number of elements. 0 creates an empty buffer.
   * @param alignment Alignment of the first element in bytes. Must be a power of two and at least the size of a pointer.
   * @throws std::invalid_argument If the alignment is invalid.
   * @throws std::bad_array_new_length If count * sizeof(T) does not fit into size_t.
   * @throws std::bad_alloc If the allocation fails.
   */
  explicit AlignedBuffer(size_t count, size_t alignment = DEFAULT_ALIGNMENT) : m_count(count), m_alignment(alignment) {
    if (!Detail::IsValidAlignment(alignment)) {
      throw std::invalid_argument("AlignedBuffer: alignment must be a power of two and at least the size of a pointer");
    }
    if (count == 0) {
      return;
    }
    if (count > std::numeric_limits<size_t>::max() / sizeof(T)) {
      throw std::bad_array_new_length();
    }
    m_data = static_cast<T*>(Detail::AllocateAligned(count * sizeof(T), alignment));
    if (m_data == nullptr) {
      throw std::bad_alloc();
    }
  }

  ~AlignedBuffer() { Detail::FreeAligned(m_data); }

  // Copying is disallowed because the buffer owns its memory exclusively.
  AlignedBuffer(const AlignedBuffer&) = delete;
  AlignedBuffer& operator=(const AlignedBuffer&) = delete;

  AlignedBuffer(AlignedBuffer&& other) noexcept
      : m_data(std::exchange(other.m_data, nullptr)),
        m_count(std::exchange(other.m_count, 0)),
        m_alignment(other.m_alignment) {}

  AlignedBuffer& operator=(AlignedBuffer&& other) noexcept {
    if (this != &other) {
      Detail::FreeAligned(m_data);
      m_data = std::exchange(other.m_data, nullptr);
      m_count = std::exchange(other.m_count, 0);
      m_alignment = other.m_alignment;
    }
    return *this;
  }

  // --- Accessors ---
  T* GetData() noexcept { return m_data; }
  const T* GetData() const noexcept { return m_data; }
  size_t GetCount() const noexcept { return m_count; }
  size_t GetSizeInBytes() const noexcept { return m_count * sizeof(T); }
  size_t GetAlignment() const noexcept { return m_alignment; }
  bool IsEmpty() const noexcept { return m_count == 0; }

  /** @brief Accesses an element without bounds checking, like std::vector::operator[]. */
  T& operator[](size_t index) noexcept { return m_data[index]; }
  const T& operator[](size_t index) const noexcept { return m_data[index]; }

  // Lowercase begin()/end() are required by range-based for loops.
  T* begin() noexcept { return m_data; }
  T* end() noexcept { return m_data + m_count; }
  const T* begin() const noexcept { return m_data; }
  const T* end() const noexcept { return m_data + m_count; }

  // --- Modifiers ---

  /** @brief Releases the memory and leaves the buffer empty. The alignment is kept. */
  void Reset() noexcept {
    Detail::FreeAligned(m_data);
    m_data = nullptr;
    m_count = 0;
  }

 private:
  // --- Storage ---
  T* m_data = nullptr;
  size_t m_count = 0;
  size_t m_alignment = DEFAULT_ALIGNMENT;
};

}  // namespace Aurivis::Simd