// Original VOLE benchmark with 1 thread (for comparison with mock)
#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"

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
const int threads = 1;  // Single-threaded

void test_vole_triple(NetIO *ios[threads + 1], int party) {
    cout << "Using REAL OT (EC) - single threaded" << endl;

    VoleTriple<NetIO> vtriple(party, threads, ios);

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

    delete[] buf;

#if defined(__linux__)
    struct rusage rusage;
    if (!getrusage(RUSAGE_SELF, &rusage))
        cout << "Peak RSS: " << (size_t)rusage.ru_maxrss << " KB" << endl;
#endif
}

int main(int argc, char **argv) {
    parse_party_and_port(argv, &party, &port);

    NetIO *ios[threads];
    for (int i = 0; i < threads; ++i)
        ios[i] = new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i);

    cout << endl;
    cout << "========================================" << endl;
    cout << "  VOLE Benchmark (Real OT, 1 thread)   " << endl;
    cout << "========================================" << endl;
    cout << endl;

    test_vole_triple(ios, party);

    for (int i = 0; i < threads; ++i)
        delete ios[i];

    return 0;
}
