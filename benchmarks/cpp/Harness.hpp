#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace Aurivis::Simd::Benchmarks {

/** @brief Receives results, so the compiler cannot remove the measured work. */
inline volatile uint64_t s_sink = 0;

/** @brief Time of one operation in nanoseconds, over several runs. */
struct Result {
  double medianNs = 0.0;
  double minNs = 0.0;
};

/**
 * @brief Measures the time per iteration of a benchmark body.
 * @param iterations Number of iterations per run.
 * @param body Callable that executes the given number of iterations.
 * @param runs Number of timed runs. One untimed warm-up run comes first.
 * @return Median and minimum time per iteration over all runs.
 *
 * @tparam Fn Callable taking the iteration count.
 */
template <typename Fn>
Result Measure(uint64_t iterations, Fn&& body, int runs = 9) {
  std::vector<double> samples;
  samples.reserve(runs);
  body(iterations / 10 + 1);
  for (int i = 0; i < runs; ++i) {
    const auto start = std::chrono::steady_clock::now();
    body(iterations);
    const auto end = std::chrono::steady_clock::now();
    samples.push_back(std::chrono::duration<double, std::nano>(end - start).count() / static_cast<double>(iterations));
  }
  std::sort(samples.begin(), samples.end());
  return {samples[samples.size() / 2], samples.front()};
}

/** @brief Prints one result line. */
inline void Print(const char* name, const Result& result) { std::printf("  %-48s %10.2f ns/op   (min %8.2f)\n", name, result.medianNs, result.minNs); }

/** @brief Prints a section title. */
inline void PrintSection(const char* title) { std::printf("\n[%s]\n", title); }

}  // namespace Aurivis::Simd::Benchmarks