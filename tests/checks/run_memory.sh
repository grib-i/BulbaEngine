#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
mode="${1:-asan}"

case "$mode" in
  asan)
    build="$root/build-tests-asan"
    cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=Debug -DBLB_BUILD_APP=ON -DBUILD_TESTING=ON -DBULBA_ENABLE_ASAN=ON
    cmake --build "$build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
    ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir "$build" -L 'unit|memory|cli' --output-on-failure
    ;;
  valgrind)
    command -v valgrind >/dev/null || { echo "Install Valgrind first." >&2; exit 1; }
    build="$root/build-tests-valgrind"
    cmake -S "$root" -B "$build" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBLB_BUILD_APP=ON -DBUILD_TESTING=ON -DBULBA_ENABLE_ASAN=OFF
    cmake --build "$build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
    cd "$root"
    status=0
    for mode in 'unit all' memory; do
      # Intentional word splitting: the two unit arguments are separate CLI words.
      valgrind --track-origins=yes --leak-check=full --show-leak-kinds=definite,indirect \
        --errors-for-leak-kinds=definite,indirect --error-exitcode=99 "$build/bulba_tests" $mode || status=1
    done
    exit "$status"
    ;;
  *)
    echo "Usage: $0 [asan|valgrind]" >&2
    exit 1
    ;;
esac
