#include TARGET_HEADER

#include "bench.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <vector>

namespace hpns = TARGET_NS;

namespace {

struct Node : hpns::hazard_pointer_obj_base<Node> { unsigned long val{0}; };

std::atomic<Node*> src{nullptr};

inline unsigned long deref(const Node* ptr) {
  return *const_cast<volatile unsigned long*>(&ptr->val);
}

// Load from src and dereference without HP protection.
[[gnu::noinline]] void run_load_only(long iters) {
  for (long i = 0; i < iters; ++i) {
    Node* ptr = src.load(std::memory_order::acquire);
    deref(ptr);
  }
}

// Load from src and dereference under HP protection, reusing the same HP object.
[[gnu::noinline]] void run_hp_protect(long iters, hpns::hazard_pointer& hp) {
  for (long i = 0; i < iters; ++i) {
    Node* ptr = hp.protect(src);
    deref(ptr);
    hp.reset_protection();
  }
}

// Load from src and dereference under HP protection, making and destroying a new
// HP object per iteration.
[[gnu::noinline]] void run_hp_protect_ctor_dtor(long iters) {
  for (long i = 0; i < iters; ++i) {
    auto hp = hpns::make_hazard_pointer();
    Node* ptr = hp.protect(src);
    deref(ptr);
  }  // hp dtor
}

struct Experiment {
  const char* impl;
  const char* op;
  int threads;
  long prepop;
  long iters;
  int reps;
};

void print_config() {
  bench::print_machine();
  bench::print_build();
  std::print("# run\n");
  bench::print_key_value("impl", TARGET_NAME);
  bench::print_key_value("impl_header", BENCH_IMPL_HEADER);
  bench::print_key_value("impl_commit", BENCH_IMPL_COMMIT);
  bench::print_key_value("date", bench::iso8601_now());
  std::print("\n");
}

void print_columns() {
  std::print("{:<10} {:<34} {:>3} {:>6} {:>10} {:>4} {:>7} {:>7} {:>7} {:>7} {:>7}\n",
             "impl", "op", "thr", "prepop", "iters", "reps",
             "min_ns", "med_ns", "avg_ns", "max_ns", "sd_ns");
  std::fflush(stdout);
}

void report(const Experiment& exp, const bench::Stats& st) {
  std::print("{:<10} {:<34} {:>3} {:>6} {:>10} {:>4}"
             " {:>7.3f} {:>7.3f} {:>7.3f} {:>7.3f} {:>7.4f}\n",
             exp.impl, exp.op, exp.threads, exp.prepop, exp.iters, exp.reps,
             st.min, st.median, st.mean, st.max, st.stddev);
  std::fflush(stdout);
}

std::vector<hpns::hazard_pointer> hp_prepop;
long max_prepop = 0;

Experiment make_experiment(const char* op, int threads, long iters, int reps) {
  long prepop = static_cast<long>(hp_prepop.size());
  if (prepop < max_prepop) {
    std::print(stderr,
               "hp_bench: prepop must not decrease ({} -> {}); "
               "an implementation may not free hazard pointers.\n",
               max_prepop, prepop);
    std::abort();
  }
  max_prepop = prepop;
  return Experiment{TARGET_NAME, op, threads, prepop, iters, reps};
}

auto no_init = [](int) { return 0; };
auto mk_init = [](int) { return hpns::make_hazard_pointer(); };

void load_only(int threads, long iters, int reps) {
  auto exp = make_experiment("Load_deref_no_protection", threads, iters, reps);
  report(exp, bench::run_bench(exp.threads, exp.iters, exp.reps, no_init,
                               [iters](int, auto&) { run_load_only(iters); }));
}

void hp_protect(int threads, long iters, int reps) {
  auto exp = make_experiment("Load_deref_HP_protection", threads, iters, reps);
  report(exp, bench::run_bench(exp.threads, exp.iters, exp.reps, mk_init,
                               [iters](int, hpns::hazard_pointer& hp) {
                                 run_hp_protect(iters, hp);
                               }));
}

void hp_protect_ctor_dtor(int threads, long iters, int reps) {
  auto exp = make_experiment("Load_deref_HP_protection_ctor_dtor", threads, iters, reps);
  report(exp, bench::run_bench(exp.threads, exp.iters, exp.reps, no_init,
                               [iters](int, auto&) {
                                 run_hp_protect_ctor_dtor(iters);
                               }));
}

}  // namespace

int main() {
  constexpr int reps = 30;
  constexpr long iters_1B = 1'000'000'000;
  constexpr long iters_100M = 100'000'000;
  constexpr long prepop = 10'000;

  Node* node = new Node;
  node->val = 123;
  src.store(node);

  print_config();
  print_columns();

  load_only(1, iters_1B, reps);
  load_only(8, iters_1B, reps);

  hp_protect(1, iters_1B, reps);
  hp_protect(8, iters_1B, reps);

  hp_protect_ctor_dtor(1, iters_100M, reps);
  hp_protect_ctor_dtor(8, iters_100M, reps);

  // Prepopulate live hazard pointers. Everything below runs against them.
  for (long i = 0; i < prepop; ++i)
    hp_prepop.push_back(hpns::make_hazard_pointer());

  hp_protect(1, iters_1B, reps);
  hp_protect(8, iters_1B, reps);

  hp_protect_ctor_dtor(1, iters_100M, reps);
  hp_protect_ctor_dtor(8, iters_100M, reps);

  return 0;
}
