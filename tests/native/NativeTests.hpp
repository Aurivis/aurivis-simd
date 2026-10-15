#pragma once

#include "Aurivis/Simd/Simd.hpp"

#include <atomic>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/**
 * @brief Returns the failed condition as text from the current test function.
 * @details Every test function returns an empty string on success, so the Python side can show which check failed.
 */
#define AURIVIS_CHECK(condition) \
  if (!(condition)) {            \
    return #condition;           \
  }

namespace Aurivis::Simd::Tests {

namespace Detail {

inline int DispatchGeneric(int value) noexcept { return value + 1; }
inline int DispatchSse41(int value) noexcept { return value + 2; }
inline int DispatchAvx2(int value) noexcept { return value + 3; }
inline int DispatchAvx512(int value) noexcept { return value + 4; }
inline int DispatchNeon(int value) noexcept { return value + 5; }
inline int DispatchSve(int value) noexcept { return value + 6; }
inline int DispatchSve2(int value) noexcept { return value + 7; }

using DispatchFn = int (*)(int) noexcept;

// One function per target attribute. Only the one Select() picks is ever called.
AURIVIS_NOINLINE inline int TargetGeneric(int value) noexcept { return value + 1; }
AURIVIS_TARGET_SSE41 AURIVIS_NOINLINE inline int TargetSse41(int value) noexcept { return value + 2; }
AURIVIS_TARGET_AVX2 AURIVIS_NOINLINE inline int TargetAvx2(int value) noexcept { return value + 3; }
AURIVIS_TARGET_AVX512 AURIVIS_NOINLINE inline int TargetAvx512(int value) noexcept { return value + 4; }
AURIVIS_TARGET_NEON AURIVIS_NOINLINE inline int TargetNeon(int value) noexcept { return value + 5; }
AURIVIS_TARGET_SVE AURIVIS_NOINLINE inline int TargetSve(int value) noexcept { return value + 6; }
AURIVIS_TARGET_SVE2 AURIVIS_NOINLINE inline int TargetSve2(int value) noexcept { return value + 7; }

}  // namespace Detail

// --- Level ---

/** @brief Every level name parses back to the same level, case-insensitively. */
inline std::string LevelNamesRoundTrip() {
  for (const Level level : ALL_LEVELS) {
    Level parsed = Level::Generic;
    AURIVIS_CHECK(TryParseLevel(LevelToString(level), parsed));
    AURIVIS_CHECK(parsed == level);
  }
  Level parsed = Level::Generic;
  AURIVIS_CHECK(TryParseLevel("AVX2", parsed) && parsed == Level::Avx2);
  AURIVIS_CHECK(TryParseLevel("Sve2", parsed) && parsed == Level::Sve2);
  AURIVIS_CHECK(!TryParseLevel("bogus", parsed));
  AURIVIS_CHECK(!TryParseLevel("", parsed));
  return {};
}

/** @brief IsSupportedBy() orders the levels of each family and never mixes families. */
inline std::string SupportRules() {
  AURIVIS_CHECK(IsSupportedBy(Level::Generic, Level::Neon));
  AURIVIS_CHECK(IsSupportedBy(Level::Sse41, Level::Avx512));
  AURIVIS_CHECK(!IsSupportedBy(Level::Avx512, Level::Avx2));
  AURIVIS_CHECK(IsSupportedBy(Level::Neon, Level::Sve2));
  AURIVIS_CHECK(IsSupportedBy(Level::Sve, Level::Sve2));
  AURIVIS_CHECK(!IsSupportedBy(Level::Sve2, Level::Sve));
  AURIVIS_CHECK(!IsSupportedBy(Level::Neon, Level::Avx512));
  AURIVIS_CHECK(!IsSupportedBy(Level::Sve, Level::Avx512));
  AURIVIS_CHECK(!IsSupportedBy(Level::Avx2, Level::Neon));
  AURIVIS_CHECK(!IsSupportedBy(Level::Avx2, Level::Sve2));
  return {};
}

// --- Detection ---

/** @brief A single missing feature lowers the best level. */
inline std::string BestLevelMapping() {
  CpuFeatures features;
  AURIVIS_CHECK(features.GetBestLevel() == Level::Generic);
  features.sse3 = features.ssse3 = features.sse41 = features.sse42 = features.popcnt = true;
  AURIVIS_CHECK(features.GetBestLevel() == Level::Sse41);
  features.avx = features.avx2 = features.fma = features.bmi1 = features.bmi2 = features.f16c = features.lzcnt = true;
  AURIVIS_CHECK(features.GetBestLevel() == Level::Sse41);
  features.movbe = true;
  AURIVIS_CHECK(features.GetBestLevel() == Level::Avx2);
  features.avx512f = features.avx512bw = features.avx512cd = features.avx512dq = features.avx512vl = true;
  AURIVIS_CHECK(features.GetBestLevel() == Level::Avx512);
  CpuFeatures arm;
  arm.neon = arm.neonFp16 = arm.dotProd = arm.i8mm = true;
  AURIVIS_CHECK(arm.GetBestLevel() == Level::Neon);
  arm.sve2 = true;
  AURIVIS_CHECK(arm.GetBestLevel() == Level::Neon);
  arm.sve = true;
  AURIVIS_CHECK(arm.GetBestLevel() == Level::Sve2);
  arm.sve2 = false;
  AURIVIS_CHECK(arm.GetBestLevel() == Level::Sve);
  return {};
}

/** @brief Detection is cached and the active level never exceeds the best level. */
inline std::string DetectionIsCached() {
  AURIVIS_CHECK(&Detect() == &Detect());
  AURIVIS_CHECK(Detect().GetBestLevel() == GetBestLevel());
  AURIVIS_CHECK(IsSupportedBy(GetActiveLevel(), GetBestLevel()));
  AURIVIS_CHECK(GetActiveLevel() == GetActiveLevel());
  return {};
}

// --- Dispatch ---

/** @brief Missing entries fall back to the next lower level, and finally to generic. */
inline std::string DispatchFallback() {
  const DispatchTable<Detail::DispatchFn> table{.generic = &Detail::DispatchGeneric, .sse41 = &Detail::DispatchSse41, .avx512 = &Detail::DispatchAvx512};
  AURIVIS_CHECK(table.SelectFor(Level::Avx512) == &Detail::DispatchAvx512);
  AURIVIS_CHECK(table.SelectFor(Level::Avx2) == &Detail::DispatchSse41);
  AURIVIS_CHECK(table.SelectFor(Level::Sse41) == &Detail::DispatchSse41);
  AURIVIS_CHECK(table.SelectFor(Level::Generic) == &Detail::DispatchGeneric);
  AURIVIS_CHECK(table.SelectFor(Level::Neon) == &Detail::DispatchGeneric);
  AURIVIS_CHECK(table.SelectFor(Level::Sve2) == &Detail::DispatchGeneric);
  const DispatchTable<Detail::DispatchFn> arm{.generic = &Detail::DispatchGeneric, .neon = &Detail::DispatchNeon, .sve2 = &Detail::DispatchSve2};
  AURIVIS_CHECK(arm.SelectFor(Level::Sve2) == &Detail::DispatchSve2);
  AURIVIS_CHECK(arm.SelectFor(Level::Sve) == &Detail::DispatchNeon);
  AURIVIS_CHECK(arm.SelectFor(Level::Neon) == &Detail::DispatchNeon);
  AURIVIS_CHECK(arm.SelectFor(Level::Avx512) == &Detail::DispatchGeneric);
  const DispatchTable<Detail::DispatchFn> sveOnly{.generic = &Detail::DispatchGeneric, .sve = &Detail::DispatchSve};
  AURIVIS_CHECK(sveOnly.SelectFor(Level::Sve2) == &Detail::DispatchSve);
  AURIVIS_CHECK(sveOnly.SelectFor(Level::Neon) == &Detail::DispatchGeneric);
  return {};
}

/** @brief Select() matches SelectFor(GetActiveLevel()) and stays the same after caching. */
inline std::string DispatchSelect() {
  const DispatchTable<Detail::DispatchFn> table{.generic = &Detail::DispatchGeneric, .sse41 = &Detail::DispatchSse41, .avx2 = &Detail::DispatchAvx2, .avx512 = &Detail::DispatchAvx512};
  const Detail::DispatchFn first = table.Select();
  AURIVIS_CHECK(first == table.SelectFor(GetActiveLevel()));
  AURIVIS_CHECK(table.Select() == first);
  AURIVIS_CHECK(first(0) >= 1);
  return {};
}

/** @brief Every target attribute compiles, and the function that Select() picks runs on this machine. */
inline std::string TargetAttributes() {
  const DispatchTable<Detail::DispatchFn> table{
    .generic = &Detail::TargetGeneric, .sse41 = &Detail::TargetSse41, .avx2 = &Detail::TargetAvx2, .avx512 = &Detail::TargetAvx512, .neon = &Detail::TargetNeon, .sve = &Detail::TargetSve, .sve2 = &Detail::TargetSve2};
  const Detail::DispatchFn expected = table.SelectFor(GetActiveLevel());
  AURIVIS_CHECK(table.Select() == expected);
  AURIVIS_CHECK(table.Select()(0) == expected(0));
  return {};
}

// --- AlignedBuffer ---

/** @brief Alignment, size, element access, moving and resetting. */
inline std::string AlignedBufferBasics() {
  for (const size_t alignment : {8u, 16u, 32u, 64u, 128u, 4096u}) {
    AlignedBuffer<float> buffer(1000, alignment);
    AURIVIS_CHECK(Simd::Detail::IsAligned(buffer.GetData(), alignment));
    AURIVIS_CHECK(buffer.GetCount() == 1000 && buffer.GetSizeInBytes() == 4000 && buffer.GetAlignment() == alignment);
  }
  AlignedBuffer<int32_t> source(16);
  for (size_t i = 0; i < source.GetCount(); ++i) {
    source[i] = static_cast<int32_t>(i);
  }
  AlignedBuffer<int32_t> moved(std::move(source));
  AURIVIS_CHECK(source.IsEmpty() && source.GetData() == nullptr);
  int32_t sum = 0;
  for (const int32_t value : moved) {
    sum += value;
  }
  AURIVIS_CHECK(sum == 120);
  moved.Reset();
  AURIVIS_CHECK(moved.IsEmpty());
  AlignedBuffer<int32_t> empty(0);
  AURIVIS_CHECK(empty.IsEmpty() && empty.GetData() == nullptr);
  return {};
}

/** @brief Invalid alignments and overflowing sizes throw. */
inline std::string AlignedBufferErrors() {
  bool threwInvalid = false;
  try {
    AlignedBuffer<int32_t> invalid(4, 3);
  } catch (const std::invalid_argument&) {
    threwInvalid = true;
  }
  AURIVIS_CHECK(threwInvalid);
  bool threwOverflow = false;
  try {
    AlignedBuffer<int64_t> huge(std::numeric_limits<size_t>::max() / 2);
  } catch (const std::bad_array_new_length&) {
    threwOverflow = true;
  }
  AURIVIS_CHECK(threwOverflow);
  return {};
}

// --- Pool ---

/** @brief Returned buffers are reused, Reserve() and ReleaseIdle() manage idle buffers. */
inline std::string PoolReuse() {
  Pool<float> pool(1000, 128);
  AURIVIS_CHECK(pool.GetCount() == 1000 && pool.GetAlignment() == 128 && pool.GetIdleCount() == 0);
  const float* first = nullptr;
  {
    auto buffer = pool.Acquire();
    first = buffer.GetData();
    AURIVIS_CHECK(Simd::Detail::IsAligned(first, 128) && buffer.GetCount() == 1000);
  }
  AURIVIS_CHECK(pool.GetIdleCount() == 1);
  {
    auto buffer = pool.Acquire();
    AURIVIS_CHECK(buffer.GetData() == first);
    auto moved = std::move(buffer);
    AURIVIS_CHECK(buffer.IsEmpty() && !moved.IsEmpty());
    moved.Reset();
    AURIVIS_CHECK(moved.IsEmpty() && pool.GetIdleCount() == 1);
  }
  pool.Reserve(4);
  AURIVIS_CHECK(pool.GetIdleCount() == 4);
  pool.ReleaseIdle();
  AURIVIS_CHECK(pool.GetIdleCount() == 0);
  return {};
}

/** @brief A buffer stays valid after its pool is destroyed, and a zero count throws. */
inline std::string PoolLifetimeAndErrors() {
  Pool<int32_t>::Buffer survivor;
  {
    Pool<int32_t> shortLived(256);
    survivor = shortLived.Acquire();
  }
  survivor[255] = 42;
  AURIVIS_CHECK(survivor[255] == 42);
  survivor.Reset();
  bool threwZero = false;
  try {
    Pool<int32_t> invalid(0);
  } catch (const std::invalid_argument&) {
    threwZero = true;
  }
  AURIVIS_CHECK(threwZero);
  return {};
}

/** @brief Eight threads acquire, write and return buffers without ever sharing one. */
inline std::string PoolThreads() {
  constexpr int THREAD_COUNT = 8;
  constexpr int ITERATIONS = 20'000;
  Pool<int32_t> pool(1024);
  std::atomic<int> corrupted{0};
  std::vector<std::thread> threads;
  for (int id = 0; id < THREAD_COUNT; ++id) {
    threads.emplace_back([&pool, &corrupted, id] {
      for (int i = 0; i < ITERATIONS; ++i) {
        auto buffer = pool.Acquire();
        buffer[0] = id;
        buffer[1023] = id;
        std::this_thread::yield();
        if (buffer[0] != id || buffer[1023] != id) {
          corrupted.fetch_add(1, std::memory_order_relaxed);
        }
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  AURIVIS_CHECK(corrupted.load() == 0);
  AURIVIS_CHECK(pool.GetIdleCount() >= 1);
  return {};
}

/** @brief Buffers acquired on one thread and returned on another are reused. */
inline std::string PoolCrossThreadReturn() {
  Pool<uint8_t> pool(4096);
  std::vector<Pool<uint8_t>::Buffer> buffers;
  for (int i = 0; i < 4; ++i) {
    buffers.push_back(pool.Acquire());
  }
  std::thread consumer([&buffers] { buffers.clear(); });
  consumer.join();
  AURIVIS_CHECK(pool.GetIdleCount() == 4);
  auto reused = pool.Acquire();
  AURIVIS_CHECK(pool.GetIdleCount() == 3 && reused.GetCount() == 4096);
  return {};
}

}  // namespace Aurivis::Simd::Tests