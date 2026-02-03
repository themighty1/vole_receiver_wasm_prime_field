// Benchmark VOLE with mock OT and BLAKE3 hash (no OpenSSL dependency)
// This benchmarks the core VOLE computation: LPN + MPFSS

// IMPORTANT: Include mock header FIRST to override Hash and OTCO
#include "emp-zk/emp-vole/emp-vole-mock.h"

#if defined(__linux__)
#include <sys/time.h>
#include <sys/resource.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <sys/resource.h>
#include <mach/mach.h>
#endif

using namespace emp;
using namespace std;

int party, port;
const int threads = 1;  // Single-threaded for WASM compatibility

void test_vole_triple_mock(CountingNetIO *ios[threads + 1], int party) {
    cout << "Using BLAKE3 GGM + Mock VOLE - single threaded" << endl;

    reset_byte_counters();
    VoleTripleBlake3<CountingNetIO> vtriple(party, threads, ios);

    __uint128_t Delta = (__uint128_t)0;
    if (party == ALICE) {
        PRG prg;
        prg.random_data(&Delta, sizeof(__uint128_t));
        Delta = Delta & ((__uint128_t)0xFFFFFFFFFFFFFFFFLL);
        Delta = mod(Delta, pr);

        auto start = clock_start();
        vtriple.setup(Delta);
        double setup_time = time_from(start) / 1000;
        cout << "Setup time: " << setup_time << " ms" << endl;
    } else {
        auto start = clock_start();
        vtriple.setup();
        double setup_time = time_from(start) / 1000;
        cout << "Setup time: " << setup_time << " ms" << endl;
    }

    // Benchmark extension
    int triple_need = vtriple.ot_limit;
    cout << "Generating " << triple_need << " VOLEs..." << endl;

    __uint128_t *buf = new __uint128_t[triple_need];

    auto start = clock_start();
    vtriple.extend(buf, triple_need);
    double extend_time = time_from(start) / 1000;

    double voles_per_sec = (double)triple_need / (extend_time / 1000.0);
    cout << "Extension time: " << extend_time << " ms" << endl;
    cout << "VOLEs generated: " << triple_need << endl;
    cout << "Rate: " << (voles_per_sec / 1e6) << " million VOLEs/sec" << endl;

    // Bootstrap stats from params
    int bootstrap_svoles = 1 + vtriple.param.t_pre0 + vtriple.param.k_pre0;
    cout << "Bootstrap sVOLEs: " << bootstrap_svoles << endl;
    cout << "OTs consumed: " << vtriple.get_ot_consumed() << endl;

    // Print communication stats (receiver only) - BEFORE verification
    if (party == BOB)
        print_comm_stats();

    // Verify correlations (not timed - sends only Delta + hash)
    cout << "Verifying correlations..." << endl;
    if (party == ALICE)
        vtriple.check_triple(Delta, buf, triple_need);
    else
        vtriple.check_triple(0, buf, triple_need);

    delete[] buf;

    // Memory usage
#if defined(__linux__)
    struct rusage rusage;
    if (!getrusage(RUSAGE_SELF, &rusage))
        cout << "Peak RSS: " << (size_t)rusage.ru_maxrss << " KB" << endl;
#elif defined(__APPLE__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info,
                  &count) == KERN_SUCCESS)
        cout << "Peak RSS: " << (size_t)info.resident_size_max << " bytes" << endl;
#endif
}

int main(int argc, char **argv) {
    parse_party_and_port(argv, &party, &port);

    NetIO *raw_ios[threads];
    CountingNetIO *ios[threads];
    for (int i = 0; i < threads; ++i) {
        raw_ios[i] = new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i);
        ios[i] = new CountingNetIO(raw_ios[i]);
    }

    cout << endl;
    cout << "========================================" << endl;
    cout << "  VOLE Benchmark (Mock OT, 1 thread)  " << endl;
    cout << "========================================" << endl;
    cout << endl;

    test_vole_triple_mock(ios, party);

    for (int i = 0; i < threads; ++i) {
        delete ios[i];
        delete raw_ios[i];
    }

    return 0;
}
