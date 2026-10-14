#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "BenchHighway.cpp"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace Aurivis::Simd::Benchmarks {
namespace HWY_NAMESPACE {

HWY_NOINLINE uint64_t Add(uint64_t value) { return value + 1; }

}  // namespace HWY_NAMESPACE
}  // namespace Aurivis::Simd::Benchmarks
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

#include "Harness.hpp"
#include "hwy/aligned_allocator.h"
#include "hwy/targets.h"

namespace Aurivis::Simd::Benchmarks {

HWY_EXPORT(Add);

HWY_NOINLINE uint64_t AddDirect(uint64_t value) { return value + 1; }

constexpr size_t FRAME_1080P = 1920 * 1080 * 3 / 2;

template <typename T>
Result MeasureAllocation(size_t count, uint64_t iterations) {
  return Measure(iterations, [count](uint64_t n) {
    for (uint64_t i = 0; i < n; ++i) {
      auto buffer = hwy::AllocateAligned<T>(count);
      s_sink = s_sink + reinterpret_cast<uintptr_t>(buffer.get());
    }
  });
}

}  // namespace Aurivis::Simd::Benchmarks

int main(int argc, char** argv) {
  using namespace Aurivis::Simd::Benchmarks;

  const bool coldOnly = argc > 1 && std::strcmp(argv[1], "--cold") == 0;

  const auto coldStart = std::chrono::steady_clock::now();
  const int64_t targets = hwy::SupportedTargets();
  const double coldNs = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - coldStart).count();
  if (coldOnly) {
    std::printf("first call %.0f ns\n", coldNs);
    return 0;
  }

  std::printf("Highway %d.%d.%d | best target: %s\n", HWY_MAJOR, HWY_MINOR, HWY_PATCH, hwy::TargetName(targets & -targets));

  PrintSection("detection");
  std::printf("  %-48s %10.0f ns      (single cold call)\n", "hwy::SupportedTargets() first call", coldNs);
  Print("hwy::SupportedTargets() repeated", Measure(5'000'000, [](uint64_t n) {
          for (uint64_t i = 0; i < n; ++i) {
            s_sink = s_sink + static_cast<uint64_t>(hwy::SupportedTargets());
          }
        }));

  PrintSection("dispatch");
  Print("direct call (baseline)", Measure(200'000'000, [](uint64_t n) {
          uint64_t value = 0;
          for (uint64_t i = 0; i < n; ++i) {
            value = AddDirect(value);
          }
          s_sink = value;
        }));
  Print("HWY_DYNAMIC_DISPATCH per call", Measure(200'000'000, [](uint64_t n) {
          uint64_t value = 0;
          for (uint64_t i = 0; i < n; ++i) {
            value = HWY_DYNAMIC_DISPATCH(Add)(value);
          }
          s_sink = value;
        }));
  Print("HWY_DYNAMIC_POINTER cached", Measure(200'000'000, [](uint64_t n) {
          auto* fn = HWY_DYNAMIC_POINTER(Add);
          uint64_t value = 0;
          for (uint64_t i = 0; i < n; ++i) {
            value = fn(value);
          }
          s_sink = value;
        }));

  PrintSection("aligned allocation: allocate + free");
  Print("hwy::AllocateAligned<int16_t>(1024)        2 KB", MeasureAllocation<int16_t>(1024, 5'000'000));
  Print("hwy::AllocateAligned<uint8_t>(65536)      64 KB", MeasureAllocation<uint8_t>(65536, 2'000'000));
  Print("hwy::AllocateAligned<uint8_t>(3110400)   1080p", MeasureAllocation<uint8_t>(FRAME_1080P, 200'000));
  return 0;
}

#endif  // HWY_ONCE