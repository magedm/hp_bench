#!/usr/bin/env bash
set -euo pipefail

# Benchmark target — edit these to benchmark another C++26 hazard-pointer impl:
header="../mm_hp/mm_hp.hpp"
ns="std"                      # namespace the HP API lives in
name="mm_hp"                  # label shown in the results

cxx="${CXX:-g++-15}"
std="c++26"
opt=(-O3 -DNDEBUG -falign-loops=32 -falign-functions=32)  # stabilize tight-loop alignment

git_commit() {
  local id dirty
  id=$(git -C "$1" describe --tags --always 2>/dev/null) || { echo unknown; return; }
  dirty=$(git -C "$1" status --porcelain | cut -c4- | tr '\n' ' ')
  [ -n "$dirty" ] && id="$id (dirty: ${dirty% })"
  echo "$id"
}

flags=(-std=$std -Wall -Wextra "${opt[@]}"
       -DTARGET_HEADER="\"$header\"" -DTARGET_NS="$ns" -DTARGET_NAME="\"$name\""
       -DBENCH_CXX="\"$cxx\"" -DBENCH_STD="\"$std\"" -DBENCH_FLAGS="\"${opt[*]}\""
       -DBENCH_CPU="\"$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//')\""
       -DBENCH_CORES="$(nproc --all)"
       -DBENCH_KERNEL="\"$(uname -r)\""
       -DBENCH_IMPL_HEADER="\"$(basename "$header")\""
       -DBENCH_IMPL_COMMIT="\"$(git_commit "$(dirname "$header")")\""
       -DBENCH_COMMIT="\"$(git_commit .)\"")

mkdir -p build
"$cxx" "${flags[@]}" -pthread hp_bench.cpp -o build/hp_bench
echo "built: build/hp_bench  ($cxx; $name)"
