#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstring>
#include <vector>

extern "C" {
#include "balloc.h"
}

static void BM_Setup(benchmark::State &state) {
    for (auto _ : state) {
        balloc_setup();
        state.PauseTiming();
        balloc_teardown();
        state.ResumeTiming();
    }
}
BENCHMARK(BM_Setup);
static void BM_Teardown(benchmark::State &state) {
    for (auto _ : state) {
        state.PauseTiming();
        balloc_setup();
        state.ResumeTiming();
        balloc_teardown();
    }
}
BENCHMARK(BM_Teardown);

static void BM_SetupTeardown(benchmark::State &state) {
    for (auto _ : state) {
        balloc_setup();
        balloc_teardown();
    }
}
BENCHMARK(BM_SetupTeardown);


BENCHMARK_MAIN();
