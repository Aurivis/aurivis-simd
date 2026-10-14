#include "Aurivis/Simd/Simd.hpp"

#include "Harness.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace Aurivis::Simd::Benchmarks {

using AddFn = uint64_t (*)(uint64_t);

// Different bodies, so the linker cannot fold the implementations into one function.
AURIVIS_NOINLINE uint64_t AddGeneric(uint64_t value) { return value + 1; }
AURIVIS_NOINLINE AURIVIS_TARGET_SSE41 uint64_t AddSse41(uint64_t value) { return value + 2; }
AURIVIS_NOINLINE AURIVIS_TARGET_AVX2 uint64_t AddAvx2(uint64_t value) { return value + 3; }
AURIVIS_NOINLINE AURIVIS_TARGET_AVX512 uint64_t AddAvx512(uint64_t value) { return value + 4; }

const DispatchTable<AddFn> ADD_TABLE{.generic = &AddGeneric, .sse41 = &AddSse41, .avx2 = &AddAvx2, .avx512 = &AddAvx512};

constexpr size_t FRAME_1080P = 1920 * 1080 * 3 / 2;

// --- Detection ---

void RunDetection(double coldNs) {
  PrintSection("detection");
  std::printf("  %-48s %10.0f ns      (single cold call)\n", "Aurivis::Simd::Detect() first call", coldNs);
  Print("loop + sink only (harness baseline)", Measure(50'000'000, [](uint64_t n) {
          for (uint64_t i = 0; i < n; ++i) {
            s_sink = s_sink + 1;
          }
        }));
  Print("Aurivis::Simd::Detect() cached", Measure(50'000'000, [](uint64_t n) {
          for (uint64_t i = 0; i < n; ++i) {
            s_sink = s_sink + static_cast<uint64_t>(Detect().avx2);
          }
        }));
  Print("Aurivis::Simd::GetActiveLevel() cached", Measure(50'000'000, [](uint64_t n) {
          for (uint64_t i = 0; i < n; ++i) {
            s_sink = s_sink + static_cast<uint64_t>(GetActiveLevel());
          }
        }));
  Print("Detail::DetectFeatures() uncached", Measure(200'000, [](uint64_t n) {
          for (uint64_t i = 0; i < n; ++i) {
            s_sink = s_sink + static_cast<uint64_t>(Detail::DetectFeatures().avx2);
          }
        }));
}

// --- Dispatch ---

void RunDispatch() {
  PrintSection("dispatch");
  Print("direct call (baseline)", Measure(200'000'000, [](uint64_t n) {
          uint64_t value = 0;
          for (uint64_t i = 0; i < n; ++i) {
            value = AddGeneric(value);
          }
          s_sink = value;
        }));
  Print("DispatchTable::Select() per call", Measure(200'000'000, [](uint64_t n) {
          uint64_t value = 0;
          for (uint64_t i = 0; i < n; ++i) {
            value = ADD_TABLE.Select()(value);
          }
          s_sink = value;
        }));
  Print("DispatchTable::Select() cached", Measure(200'000'000, [](uint64_t n) {
          const AddFn fn = ADD_TABLE.Select();
          uint64_t value = 0;
          for (uint64_t i = 0; i < n; ++i) {
            value = fn(value);
          }
          s_sink = value;
        }));
}

// --- Memory ---

template <typename T>
Result MeasureAllocation(size_t count, uint64_t iterations) {
  return Measure(iterations, [count](uint64_t n) {
    for (uint64_t i = 0; i < n; ++i) {
      AlignedBuffer<T> buffer(count);
      s_sink = s_sink + reinterpret_cast<uintptr_t>(buffer.GetData());
    }
  });
}

template <typename T>
Result MeasurePool(Pool<T>& pool, uint64_t iterations) {
  return Measure(iterations, [&pool](uint64_t n) {
    for (uint64_t i = 0; i < n; ++i) {
      auto buffer = pool.Acquire();
      s_sink = s_sink + reinterpret_cast<uintptr_t>(buffer.GetData());
    }
  });
}

void RunMemory() {
  PrintSection("aligned allocation: allocate + free");
  Print("AlignedBuffer<int16_t>(1024)              2 KB", MeasureAllocation<int16_t>(1024, 5'000'000));
  Print("AlignedBuffer<uint8_t>(65536)            64 KB", MeasureAllocation<uint8_t>(65536, 2'000'000));
  Print("AlignedBuffer<uint8_t>(3110400)         1080p", MeasureAllocation<uint8_t>(FRAME_1080P, 200'000));

  PrintSection("pool: acquire + return");
  Pool<int16_t> small(1024);
  Pool<uint8_t> medium(65536);
  Pool<uint8_t> frame(FRAME_1080P);
  Print("Pool<int16_t>(1024).Acquire()              2 KB", MeasurePool(small, 20'000'000));
  Print("Pool<uint8_t>(65536).Acquire()            64 KB", MeasurePool(medium, 20'000'000));
  Print("Pool<uint8_t>(3110400).Acquire()         1080p", MeasurePool(frame, 20'000'000));
}

// --- Threads ---

template <typename Fn>
double MeasureThreads(int threadCount, int iterations, Fn&& body) {
  const auto start = std::chrono::steady_clock::now();
  std::vector<std::thread> threads;
  for (int id = 0; id < threadCount; ++id) {
    threads.emplace_back([&body, id, iterations] {
      for (int i = 0; i < iterations; ++i) {
        body(id);
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
  return ns / (threadCount * static_cast<double>(iterations));
}

void RunContention() {
  constexpr int ITERATIONS = 500'000;
  for (const int threadCount : {2, 4, 8}) {
    std::printf("\n[contended: %d threads, acquire + write + return, 2 KB]\n", threadCount);
    Pool<int16_t> pool(1024);
    const double poolNs = MeasureThreads(threadCount, ITERATIONS, [&pool](int id) {
      auto buffer = pool.Acquire();
      buffer[0] = static_cast<int16_t>(id);
      buffer[1023] = static_cast<int16_t>(id);
    });
    const double heapNs = MeasureThreads(threadCount, ITERATIONS, [](int id) {
      AlignedBuffer<int16_t> buffer(1024);
      buffer[0] = static_cast<int16_t>(id);
      s_sink = s_sink + static_cast<uint64_t>(buffer[0]);
    });
    std::printf("  %-48s %10.2f ns/op   (all threads together)\n", "Pool<int16_t>(1024).Acquire()", poolNs);
    std::printf("  %-48s %10.2f ns/op   (all threads together)\n", "AlignedBuffer<int16_t>(1024)", heapNs);
  }
}

void RunProducerConsumer() {
  PrintSection("producer/consumer: decode thread acquires, render thread returns, 1080p");
  constexpr int FRAMES = 200'000;
  constexpr size_t QUEUE_LIMIT = 8;
  Pool<uint8_t> pool(FRAME_1080P);
  std::mutex mutex;
  std::condition_variable changed;
  std::deque<Pool<uint8_t>::Buffer> queue;
  const auto start = std::chrono::steady_clock::now();
  std::thread producer([&] {
    for (int i = 0; i < FRAMES; ++i) {
      auto frame = pool.Acquire();
      frame[0] = static_cast<uint8_t>(i);
      {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [&] { return queue.size() < QUEUE_LIMIT; });
        queue.push_back(std::move(frame));
      }
      changed.notify_all();
    }
  });
  std::thread consumer([&] {
    for (int i = 0; i < FRAMES; ++i) {
      Pool<uint8_t>::Buffer frame;
      {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [&] { return !queue.empty(); });
        frame = std::move(queue.front());
        queue.pop_front();
      }
      changed.notify_all();
      s_sink = s_sink + frame[0];
    }
  });
  producer.join();
  consumer.join();
  const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
  std::printf("  %d frames through an 8-frame queue: %.0f ns/frame (queue included) | buffers allocated: %zu\n", FRAMES, ns / FRAMES, pool.GetIdleCount());
}

}  // namespace Aurivis::Simd::Benchmarks

int main(int argc, char** argv) {
  using namespace Aurivis::Simd;
  using namespace Aurivis::Simd::Benchmarks;

  const bool coldOnly = argc > 1 && std::strcmp(argv[1], "--cold") == 0;

  const auto coldStart = std::chrono::steady_clock::now();
  const CpuFeatures& features = Detect();
  const double coldNs = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - coldStart).count();
  if (coldOnly) {
    std::printf("first call %.0f ns\n", coldNs);
    return 0;
  }

  std::printf("aurivis-simd %d.%d.%d | best level: %s | active level: %s | avx2=%d avx512f=%d\n",
              AURIVIS_SIMD_VERSION_MAJOR,
              AURIVIS_SIMD_VERSION_MINOR,
              AURIVIS_SIMD_VERSION_PATCH,
              LevelToString(GetBestLevel()),
              LevelToString(GetActiveLevel()),
              features.avx2,
              features.avx512f);
  RunDetection(coldNs);
  RunDispatch();
  RunMemory();
  RunContention();
  RunProducerConsumer();
  return 0;
}