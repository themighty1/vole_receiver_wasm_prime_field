#ifndef EMP_VOLE_MOCK_H__
#define EMP_VOLE_MOCK_H__

/*
 * Mock VOLE headers - NO OpenSSL, NO AES, NO emp-ot dependency
 * Pure BLAKE3-based implementation for WASM compatibility
 *
 * What's mocked:
 * - COT generation (BaseCotMock) - synchronized PRG instead of IKNP
 * - Bootstrap sVOLEs (Base_svole_direct_mock) - synchronized PRG
 *
 * What's real:
 * - MPFSS/SPFSS protocol (GGM trees with real communication)
 * - LPN expansion (BLAKE3-based index generation)
 * - OTPre protocol (real communication for MPFSS corrections)
 *
 * WARNING: Mock implementations are NOT SECURE - for benchmarking only!
 */

// Standard library includes
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>

// Use std:: explicitly
using std::min;
using std::max;
using std::vector;

// Override Hash class with BLAKE3 version BEFORE any other includes
#define EMP_HASH_H__  // Prevent original Hash from being included

// Guard out OpenSSL-dependent emp-tool components
#define EMP_TOOL_WASM
#ifndef EMP_AES_OPT_KS_H__
#define EMP_AES_OPT_KS_H__  // Skip aes_opt.h (uses AES-NI ParaEnc)
#endif
#ifndef EMP_MITCCRH_H__
#define EMP_MITCCRH_H__  // Skip mitccrh.h (uses AES)
#endif

// Include BLAKE3 Hash first (before emp-tool)
#include "emp-zk/emp-vole/hash_blake3.h"

// For WASM: Single-threaded ThreadPool stub (no pthreads)
#ifdef __EMSCRIPTEN__
#define EMP_THREAD_POOL_H  // Skip real ThreadPool
#include <future>
#include <functional>

class ThreadPool {
public:
    ThreadPool(size_t) {}
    ~ThreadPool() {}
    int size() const { return 1; }

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type> {
        using return_type = typename std::result_of<F(Args...)>::type;
        // Execute synchronously
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        (*task)();  // Execute immediately
        return res;
    }
};
#endif

// Now include minimal emp-tool (with OpenSSL deps guarded)
#include "emp-tool/emp-tool.h"

// Global byte counters for network stats
namespace emp {
    inline size_t& global_bytes_sent() { static size_t v = 0; return v; }
    inline size_t& global_bytes_recv() { static size_t v = 0; return v; }
    inline void reset_byte_counters() { global_bytes_sent() = 0; global_bytes_recv() = 0; }
    inline void print_comm_stats() {
        size_t sent = global_bytes_sent();
        size_t recv = global_bytes_recv();
        std::cout << "Communication:" << std::endl;
        std::cout << "  Sent: " << sent << " bytes (" << (sent / 1024.0) << " KB)" << std::endl;
        std::cout << "  Recv: " << recv << " bytes (" << (recv / 1024.0) << " KB)" << std::endl;
    }
}

// Instrumented NetIO that counts bytes
namespace emp {
class CountingNetIO : public IOChannel<CountingNetIO> {
public:
    NetIO *io;

    CountingNetIO(NetIO *io) : io(io) {}

    void send_data_internal(const void *data, size_t len) {
        io->send_data_internal(data, len);
        global_bytes_sent() += len;
    }

    void recv_data_internal(void *data, size_t len) {
        io->recv_data_internal(data, len);
        global_bytes_recv() += len;
    }

    void flush() { io->flush(); }
};
}

// Include BLAKE3-based TwoKeyPRP for GGM tree expansion
#include "emp-zk/emp-vole/twokeyprp_blake3.h"

// Include VOLE components (use BLAKE3 preot)
#include "emp-zk/emp-vole/utility.h"
#include "emp-zk/emp-vole/base_svole_direct_mock.h"
#include "emp-zk/emp-vole/preot_blake3.h"

// Include BLAKE3-based SPFSS (uses BLAKE3 for GGM tree instead of AES)
#include "emp-zk/emp-vole/spfss_sender_blake3.h"
#include "emp-zk/emp-vole/spfss_recver_blake3.h"
#include "emp-zk/emp-vole/mpfss_reg_blake3.h"
#include "emp-zk/emp-vole/lpn_blake3.h"
#include "emp-zk/emp-vole/vole_triple_blake3.h"

#endif // EMP_VOLE_MOCK_H__
