# hp_bench

- Microbenchmarks for C++26 hazard pointer implementations, for Linux x86_64 with gcc.
- Intended to test some performance-critical aspects of implementations.
- Measures:
  - three operations:
    - load and dereference a pointer unprotected (baseline),
    - then the same under hazard pointer protection (adds the cost of hazard pointer protection),
    - then with a hazard pointer constructed and destroyed per iteration (adds the cost of constructing and destroying a nonempty hazard pointer object),
  - at 1 and 8 concurrent threads,
  - with and without 10,000 pre-existing hazard pointers.
- Targets the [mm_hp](https://github.com/magedm/mm_hp) implementation by default. Clone it alongside this repo.
- Pointing it at another implementation is a three-line edit at the top of `build.sh`.
- To build: `./build.sh`.
- To run the microbenchmarks: `./build/hp_bench`.
- The output is a configuration block followed by one row per measurement.
- Example output is under `results/`.
