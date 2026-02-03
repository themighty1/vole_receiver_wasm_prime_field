// VOLE WASM Receiver (Bob)
// Connects to native sender via WebSocket proxy

#include <cstdio>
#include <chrono>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "../emp-zk/emp-vole/emp-vole-portable.h"

using namespace emp;

#ifdef __EMSCRIPTEN__

extern "C" {

EMSCRIPTEN_KEEPALIVE
int vole_run(const char* server_ip, int port) {
    printf("\n========================================\n");
    printf("VOLE WASM Receiver (Bob)\n");
    printf("========================================\n\n");

    // Create IO array (VoleTripleBlake3 expects IO**)
    // NetIO in WASM mode uses WebSocket
    NetIO io(server_ip, port);
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

int main() {
    printf("VOLE WASM Receiver module loaded.\n");
    printf("Call vole_run('localhost', 8080) to connect to sender via WebSocket proxy.\n");
    return 0;
}

}

#else
int main(int argc, char** argv) {
    printf("Build with Emscripten for WASM.\n");
    printf("For native, use native/build/vole_receiver\n");
    return 1;
}
#endif
