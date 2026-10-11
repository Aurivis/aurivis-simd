#pragma once

#include "Aurivis/Simd/AlignedBuffer.hpp"
#include "Aurivis/Simd/Config.hpp"
#include "Aurivis/Simd/Detail/AlignedAlloc.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

#if AURIVIS_ARCH_X86_64 || (AURIVIS_ARCH_ARM64 && AURIVIS_COMPILER_MSVC)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <immintrin.h>
#endif
#endif

namespace Aurivis::Simd {

namespace Detail {

/** @brief Number of independent shards per pool. Must be a power of two. */
inline constexpr uint32_t POOL_SHARD_COUNT = 16;

/** @brief Marks a thread that has not been assigned a shard yet. */
inline constexpr uint32_t UNASSIGNED_SHARD = std::numeric_limits<uint32_t>::max();

// --- Thread-to-Shard Assignment ---
inline std::atomic<uint32_t> s_nextShard{0};
inline thread_local uint32_t s_threadShard = UNASSIGNED_SHARD;

/** @brief Gets the shard of the calling thread. Threads are assigned round-robin on their first call. */
AURIVIS_FORCE_INLINE uint32_t GetThreadShard() noexcept {
  uint32_t shard = s_threadShard;
  if (shard == UNASSIGNED_SHARD) [[unlikely]] {
    shard = s_nextShard.fetch_add(1, std::memory_order_relaxed) & (POOL_SHARD_COUNT - 1);
    s_threadShard = shard;
  }
  return shard;
}

/** @brief Tells the CPU that this thread is busy-waiting, which saves power and frees resources for the other hyperthread. */
AURIVIS_FORCE_INLINE void CpuRelax() noexcept {
#if AURIVIS_ARCH_X86_64
  _mm_pause();
#elif AURIVIS_ARCH_ARM64 && AURIVIS_COMPILER_MSVC
  __yield();
#elif AURIVIS_ARCH_ARM64
  __asm__ __volatile__("yield");
#endif
}

// Shards are cache-line aligned on purpose, which MSVC reports as padding (C4324).
#if AURIVIS_COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

/**
 * @class PoolState
 * @brief The shared state behind a Pool.
 * @details The idle buffers are spread over shards, each with its own lock, so threads that use different shards
 *          never wait for each other. Idle buffers are linked through their own first bytes, so returning a buffer
 *          never allocates. The state lives until both the pool and every buffer taken from it are gone, so a
 *          buffer may outlive its pool. All methods are thread-safe.
 */
class PoolState {
 public:
  PoolState(size_t bufferSizeInBytes, size_t alignment) noexcept : m_allocationSize(std::max(bufferSizeInBytes, sizeof(void*))), m_alignment(alignment) {}

  /**
   * @brief Creates a pool state in aligned memory.
   * @details Uses AllocateAligned() instead of the aligned operator new of C++17, which is missing on macOS before
   *          10.13. Python 3.11 for macOS still targets 10.9.
   * @throws std::bad_alloc If the allocation fails.
   */
  static PoolState* Create(size_t bufferSizeInBytes, size_t alignment) {
    void* memory = AllocateAligned(sizeof(PoolState), alignof(PoolState));
    if (memory == nullptr) {
      throw std::bad_alloc();
    }
    return new (memory) PoolState(bufferSizeInBytes, alignment);
  }

  /** @brief Destroys a state created by Create() and releases its memory. */
  void Destroy() noexcept {
    this->~PoolState();
    FreeAligned(this);
  }

  ~PoolState() {
    for (Shard& shard : m_shards) {
      FreeList(shard.head.load(std::memory_order_relaxed));
    }
  }

  // Copying is disallowed because the state owns the idle buffers.
  PoolState(const PoolState&) = delete;
  PoolState& operator=(const PoolState&) = delete;

  /**
   * @brief Takes an idle buffer, preferring the given shard, or allocates a new one.
   * @param shardIndex The shard of the calling thread. The buffer must later be returned to the same shard.
   * @throws std::bad_alloc If a new buffer is needed and the allocation fails.
   */
  void* Take(uint32_t shardIndex) {
    Shard& own = m_shards[shardIndex];
    own.Lock();
    void* buffer = own.Pop();
    ++own.outstanding;
    own.Unlock();
    if (buffer != nullptr) [[likely]] {
      return buffer;
    }

    // --- Slow path: steal from another shard, or allocate ---
    buffer = Steal(shardIndex);
    if (buffer == nullptr) {
      buffer = AllocateAligned(m_allocationSize, m_alignment);
    }
    if (buffer == nullptr) {
      own.Lock();
      --own.outstanding;
      own.Unlock();
      throw std::bad_alloc();
    }
    return buffer;
  }

  /**
   * @brief Returns a buffer to the shard it was taken from. Frees the whole state if the pool is already gone and
   *        this was the last buffer.
   */
  void Give(void* buffer, uint32_t shardIndex) noexcept {
    Shard& shard = m_shards[shardIndex];
    shard.Lock();
    if (!m_isClosed) {
      shard.Push(buffer);
      --shard.outstanding;
      shard.Unlock();
      return;
    }
    shard.Unlock();
    FreeAligned(buffer);
    if (m_remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
      Destroy();
    }
  }

  /**
   * @brief Allocates idle buffers into the given shard until the pool owns at least bufferCount buffers in total.
   * @throws std::bad_alloc If an allocation fails.
   */
  void Reserve(size_t bufferCount, uint32_t shardIndex) {
    for (size_t total = GetTotalCount(); total < bufferCount; ++total) {
      void* buffer = AllocateAligned(m_allocationSize, m_alignment);
      if (buffer == nullptr) {
        throw std::bad_alloc();
      }
      Shard& shard = m_shards[shardIndex];
      shard.Lock();
      shard.Push(buffer);
      shard.Unlock();
    }
  }

  /** @brief Frees all idle buffers. Buffers that are in use are not affected. */
  void ReleaseIdle() noexcept {
    for (Shard& shard : m_shards) {
      shard.Lock();
      void* list = shard.DetachAll();
      shard.Unlock();
      FreeList(list);
    }
  }

  /** @brief Marks the pool as gone and frees the idle buffers. Frees the state now, or when the last buffer returns. */
  void Close() noexcept {
    // Holding every shard lock makes the closed flag and the remaining count visible to every later Give().
    for (Shard& shard : m_shards) {
      shard.Lock();
    }
    m_isClosed = true;
    size_t outstanding = 0;
    void* lists[POOL_SHARD_COUNT] = {};
    for (uint32_t i = 0; i < POOL_SHARD_COUNT; ++i) {
      outstanding += m_shards[i].outstanding;
      lists[i] = m_shards[i].DetachAll();
    }
    m_remaining.store(outstanding, std::memory_order_release);
    for (Shard& shard : m_shards) {
      shard.Unlock();
    }

    for (void* list : lists) {
      FreeList(list);
    }
    if (outstanding == 0) {
      Destroy();
    }
  }

  // --- Accessors ---
  size_t GetIdleCount() noexcept {
    size_t count = 0;
    for (Shard& shard : m_shards) {
      shard.Lock();
      count += shard.idleCount;
      shard.Unlock();
    }
    return count;
  }

  size_t GetTotalCount() noexcept {
    size_t count = 0;
    for (Shard& shard : m_shards) {
      shard.Lock();
      count += shard.idleCount + shard.outstanding;
      shard.Unlock();
    }
    return count;
  }

  size_t GetAlignment() const noexcept { return m_alignment; }

 private:
  static constexpr int MAX_SPINS = 64;  // Busy-wait iterations before yielding to the scheduler

  /** @brief One independent part of the pool, on its own cache line so shards never share one. */
  struct alignas(64) Shard {
    std::atomic<bool> locked{false};
    std::atomic<void*> head{nullptr};  // Written under the lock, peeked without it
    size_t idleCount = 0;
    size_t outstanding = 0;

    void Lock() noexcept {
      while (locked.exchange(true, std::memory_order_acquire)) {
        int spins = 0;
        while (locked.load(std::memory_order_relaxed)) {
          if (++spins < MAX_SPINS) {
            CpuRelax();
          } else {
            std::this_thread::yield();
          }
        }
      }
    }

    void Unlock() noexcept { locked.store(false, std::memory_order_release); }

    void* Pop() noexcept {
      void* buffer = head.load(std::memory_order_relaxed);
      if (buffer != nullptr) {
        head.store(*static_cast<void**>(buffer), std::memory_order_relaxed);
        --idleCount;
      }
      return buffer;
    }

    void Push(void* buffer) noexcept {
      *static_cast<void**>(buffer) = head.load(std::memory_order_relaxed);
      head.store(buffer, std::memory_order_relaxed);
      ++idleCount;
    }

    void* DetachAll() noexcept {
      void* list = head.load(std::memory_order_relaxed);
      head.store(nullptr, std::memory_order_relaxed);
      idleCount = 0;
      return list;
    }
  };

  /** @brief Takes an idle buffer from any other shard. The caller's shard already counts it as outstanding. */
  void* Steal(uint32_t shardIndex) noexcept {
    for (uint32_t offset = 1; offset < POOL_SHARD_COUNT; ++offset) {
      Shard& other = m_shards[(shardIndex + offset) & (POOL_SHARD_COUNT - 1)];
      if (other.head.load(std::memory_order_relaxed) == nullptr) {
        continue;  // Cheap check without locking; most shards are empty
      }
      other.Lock();
      void* buffer = other.Pop();
      other.Unlock();
      if (buffer != nullptr) {
        return buffer;
      }
    }
    return nullptr;
  }

  static void FreeList(void* list) noexcept {
    while (list != nullptr) {
      void* next = *static_cast<void**>(list);
      FreeAligned(list);
      list = next;
    }
  }

  // --- Configuration ---
  size_t m_allocationSize = 0;
  size_t m_alignment = 0;

  // --- Runtime State ---
  Shard m_shards[POOL_SHARD_COUNT];
  bool m_isClosed = false;             // Written while holding every shard lock
  std::atomic<size_t> m_remaining{0};  // Buffers still in use after Close()
};

#if AURIVIS_COMPILER_MSVC
#pragma warning(pop)
#endif

}  // namespace Detail

/**
 * @class Pool
 * @brief Hands out aligned buffers of one fixed size and reuses them instead of freeing them.
 * @details Acquire() returns a Pool::Buffer that goes back to the pool automatically when it is destroyed. Reusing
 *          buffers avoids asking the operating system for memory on every frame. Each thread works on its own shard,
 *          so threads rarely wait for each other. A buffer may outlive its pool; it is then freed when it is
 *          destroyed. Acquire(), Reserve() and ReleaseIdle() are thread-safe, and buffers may be returned from any
 *          thread.
 *
 * @tparam T Element type. Must be trivial, e.g. an integer or floating-point type.
 */
template <typename T>
class Pool {
  static_assert(std::is_trivially_default_constructible_v<T> && std::is_trivially_destructible_v<T>, "Pool only holds trivial types such as integers and floats.");

 public:
  /**
   * @class Buffer
   * @brief A buffer borrowed from a Pool. Goes back to the pool when it is destroyed or reset.
   */
  class Buffer {
   public:
    /** @brief Creates an empty buffer that does not belong to any pool. */
    Buffer() noexcept = default;

    ~Buffer() { Reset(); }

    // Copying is disallowed because the buffer is borrowed exclusively.
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    Buffer(Buffer&& other) noexcept
        : m_data(std::exchange(other.m_data, nullptr)),
          m_count(std::exchange(other.m_count, 0)),
          m_state(std::exchange(other.m_state, nullptr)),
          m_shard(other.m_shard) {}

    Buffer& operator=(Buffer&& other) noexcept {
      if (this != &other) {
        Reset();
        m_data = std::exchange(other.m_data, nullptr);
        m_count = std::exchange(other.m_count, 0);
        m_state = std::exchange(other.m_state, nullptr);
        m_shard = other.m_shard;
      }
      return *this;
    }

    // --- Accessors ---
    T* GetData() noexcept { return m_data; }
    const T* GetData() const noexcept { return m_data; }
    size_t GetCount() const noexcept { return m_count; }
    size_t GetSizeInBytes() const noexcept { return m_count * sizeof(T); }
    bool IsEmpty() const noexcept { return m_data == nullptr; }

    /** @brief Accesses an element without bounds checking, like std::vector::operator[]. */
    T& operator[](size_t index) noexcept { return m_data[index]; }
    const T& operator[](size_t index) const noexcept { return m_data[index]; }

    // Lowercase begin()/end() are required by range-based for loops.
    T* begin() noexcept { return m_data; }
    T* end() noexcept { return m_data + m_count; }
    const T* begin() const noexcept { return m_data; }
    const T* end() const noexcept { return m_data + m_count; }

    // --- Modifiers ---

    /** @brief Returns the memory to the pool now and leaves this buffer empty. */
    void Reset() noexcept {
      if (m_state != nullptr) {
        m_state->Give(m_data, m_shard);
      }
      m_data = nullptr;
      m_count = 0;
      m_state = nullptr;
    }

   private:
    friend class Pool;

    Buffer(T* data, size_t count, Detail::PoolState* state, uint32_t shard) noexcept : m_data(data), m_count(count), m_state(state), m_shard(shard) {}

    // --- Storage ---
    T* m_data = nullptr;
    size_t m_count = 0;
    Detail::PoolState* m_state = nullptr;
    uint32_t m_shard = 0;
  };

  /**
   * @brief Creates an empty pool. No memory is allocated until the first Acquire() or Reserve().
   * @param count Number of elements in every buffer. Must not be 0.
   * @param alignment Alignment of every buffer in bytes. Must be a power of two and at least the size of a pointer.
   * @throws std::invalid_argument If count is 0 or the alignment is invalid.
   * @throws std::bad_array_new_length If count * sizeof(T) does not fit into size_t.
   */
  explicit Pool(size_t count, size_t alignment = DEFAULT_ALIGNMENT) : m_count(count) {
    if (count == 0) {
      throw std::invalid_argument("Pool: count must not be 0");
    }
    if (!Detail::IsValidAlignment(alignment)) {
      throw std::invalid_argument("Pool: alignment must be a power of two and at least the size of a pointer");
    }
    if (count > std::numeric_limits<size_t>::max() / sizeof(T)) {
      throw std::bad_array_new_length();
    }
    m_state = Detail::PoolState::Create(count * sizeof(T), alignment);
  }

  ~Pool() {
    if (m_state != nullptr) {
      m_state->Close();
    }
  }

  // Copying is disallowed because the pool owns its state exclusively.
  Pool(const Pool&) = delete;
  Pool& operator=(const Pool&) = delete;

  Pool(Pool&& other) noexcept : m_state(std::exchange(other.m_state, nullptr)), m_count(std::exchange(other.m_count, 0)) {}

  Pool& operator=(Pool&& other) noexcept {
    if (this != &other) {
      if (m_state != nullptr) {
        m_state->Close();
      }
      m_state = std::exchange(other.m_state, nullptr);
      m_count = std::exchange(other.m_count, 0);
    }
    return *this;
  }

  /**
   * @brief Borrows a buffer. Reuses an idle one if possible, otherwise allocates a new one.
   * @return The buffer. Its memory is not initialized and may contain data from a previous use.
   * @throws std::bad_alloc If a new buffer is needed and the allocation fails.
   */
  Buffer Acquire() {
    const uint32_t shard = Detail::GetThreadShard();
    return Buffer(static_cast<T*>(m_state->Take(shard)), m_count, m_state, shard);
  }

  /**
   * @brief Allocates idle buffers in advance, so later Acquire() calls never wait for the operating system.
   * @param bufferCount Total number of buffers the pool should own, including buffers that are in use.
   * @throws std::bad_alloc If an allocation fails.
   */
  void Reserve(size_t bufferCount) { m_state->Reserve(bufferCount, Detail::GetThreadShard()); }

  /** @brief Frees all idle buffers. Buffers that are in use are not affected. */
  void ReleaseIdle() noexcept { m_state->ReleaseIdle(); }

  // --- Accessors ---
  size_t GetCount() const noexcept { return m_count; }
  size_t GetAlignment() const noexcept { return m_state->GetAlignment(); }
  size_t GetIdleCount() const noexcept { return m_state->GetIdleCount(); }

 private:
  // --- Storage ---
  Detail::PoolState* m_state = nullptr;
  size_t m_count = 0;
};

}  // namespace Aurivis::Simd