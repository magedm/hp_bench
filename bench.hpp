#pragma once

#include <algorithm>
#include <barrier>
#include <chrono>
#include <cmath>
#include <format>
#include <fstream>
#include <iterator>
#include <print>
#include <sched.h>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace bench {

struct Stats {
  double min;
  double max;
  double mean;
  double median;
  double stddev;
};

template <class Init, class Body>
double run_once(int nthr, const Init& init, const Body& body) {
  std::barrier bar(nthr + 1);
  std::vector<std::jthread> pool;
  pool.reserve(nthr);
  for (int tid = 0; tid < nthr; ++tid) {
    pool.emplace_back([&, tid] {
      auto state = init(tid);
      bar.arrive_and_wait();  // ready
      bar.arrive_and_wait();  // go
      body(tid, state);
      bar.arrive_and_wait();  // done
    });
  }
  bar.arrive_and_wait();  // ready: all threads set up
  auto t0 = std::chrono::steady_clock::now();
  bar.arrive_and_wait();  // go: release the timed work
  bar.arrive_and_wait();  // done: all work finished
  auto t1 = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(t1 - t0).count();
}

inline Stats summarize(std::vector<double>& samples) {
  std::ranges::sort(samples);
  auto count = std::ssize(samples);
  double sum = 0;
  for (double x : samples) sum += x;
  double mean = sum / count;
  double var = 0;
  for (double x : samples) var += (x - mean) * (x - mean);
  var /= count;
  double median =
      (count & 1) ? samples[count / 2] : (samples[count / 2 - 1] + samples[count / 2]) / 2;
  return Stats{samples.front(), samples.back(), mean, median, std::sqrt(var)};
}

template <class Init, class Body>
Stats run_bench(int nthr, long niter, int reps, const Init& init, const Body& body) {
  run_once(nthr, init, body);  // warmup, discarded
  std::vector<double> samples;
  samples.reserve(reps);
  for (int rep = 0; rep < reps; ++rep)
    samples.push_back(run_once(nthr, init, body) / static_cast<double>(niter));
  return summarize(samples);
}

inline std::string read_line(const std::string& path) {
  std::ifstream file(path);
  std::string line;
  return std::getline(file, line) && !line.empty() ? line : "unknown";
}

inline void print_key_value(std::string_view key, std::string_view value) {
  std::print("#   {:<13} {}\n", key, value);
}

inline std::string iso8601_now() {
  auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
  return std::format("{:%FT%T%z}", std::chrono::zoned_time{std::chrono::current_zone(), now});
}

inline void print_machine() {
  cpu_set_t allowed;
  CPU_ZERO(&allowed);
  sched_getaffinity(0, sizeof allowed, &allowed);
  int first_cpu = 0;
  while (first_cpu < CPU_SETSIZE && !CPU_ISSET(first_cpu, &allowed)) ++first_cpu;
  std::string dir = std::format("/sys/devices/system/cpu/cpu{}/cpufreq/", first_cpu);
  std::string cpus;
  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu)
    if (CPU_ISSET(cpu, &allowed)) cpus += std::format("{} ", cpu);
  std::print("# machine\n");
  print_key_value("cpu", BENCH_CPU);
  print_key_value("cores", std::format("{}", BENCH_CORES));
  print_key_value("cpus_allowed", cpus);
  print_key_value("kernel", BENCH_KERNEL);
  print_key_value("clock_policy", read_line(dir + "scaling_governor"));
  print_key_value("clock_max_khz", read_line(dir + "scaling_max_freq"));
}

inline void print_build() {
  std::print("# build\n");
  print_key_value("compiler", std::format("{} -- gcc {}", BENCH_CXX, __VERSION__));
  print_key_value("std", BENCH_STD);
  print_key_value("flags", BENCH_FLAGS);
  print_key_value("bench_commit", BENCH_COMMIT);
}

}  // namespace bench
