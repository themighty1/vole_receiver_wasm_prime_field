// VOLE Receiver (Bob)
// Holds x and z values where z = x*Delta + y

#include <cstdio>
#include <chrono>

#include "../emp-zk/emp-vole/emp-vole-portable.h"

using namespace emp;

int main(int argc, char** argv) {
    const char* sender_ip = "127.0.0.1";
    int port = 12345;
    if (argc > 1) sender_ip = argv[1];
    if (argc > 2) port = atoi(argv[2]);

    printf("\n========================================\n");
    printf("VOLE Receiver (Bob)\n");
    printf("========================================\n\n");

    // Receiver connects to sender
    NetIO io(sender_ip, port);
    NetIO* ios[1] = {&io};

    printf("--- Setup Phase ---\n");
    auto setup_start = std::chrono::high_resolution_clock::now();

    VoleTripleBlake3<NetIO> vole(BOB, 1, ios);
    vole.setup();

    auto setup_end = std::chrono::high_resolution_clock::now();
    auto setup_ms = std::chrono::duration_cast<std::chrono::milliseconds>(setup_end - setup_start).count();
    printf("Setup time: %lld ms\n", (long long)setup_ms);
    printf("  Stage 0: 1821 base -> 9600\n");
    printf("  Stage 1: 9600 -> 166400\n\n");

    // Extend
    printf("--- Extension Phase ---\n");
    int64_t output_size = vole.param.buf_sz();
    printf("Target: %lld VOLEs\n", (long long)output_size);

    std::vector<__uint128_t> voles(vole.param.n);

    auto extend_start = std::chrono::high_resolution_clock::now();
    vole.extend(voles.data());
    auto extend_end = std::chrono::high_resolution_clock::now();
    auto extend_ms = std::chrono::duration_cast<std::chrono::milliseconds>(extend_end - extend_start).count();

    printf("  Final: 166400 -> 10168320\n");
    printf("Extension time: %lld ms\n\n", (long long)extend_ms);

    double rate = (double)output_size / ((setup_ms + extend_ms) / 1000.0);

    printf("========================================\n");
    printf("Results\n");
    printf("========================================\n");
    printf("Total time:      %lld ms\n", (long long)(setup_ms + extend_ms));
    printf("VOLEs generated: %lld\n", (long long)output_size);
    printf("Rate: %.2f million VOLEs/sec\n", rate / 1e6);
    printf("========================================\n\n");

    io.print_stats();

    printf("\n--- Mock Statistics ---\n");
    printf("Base COTs:   %lld\n", (long long)BaseCotMock<NetIO>::total_cots);
    printf("Base VOLEs:  %lld\n", (long long)Base_svole_direct_mock<NetIO>::total_base_voles);

    return 0;
}
