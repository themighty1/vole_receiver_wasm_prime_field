#ifndef EMP_TOOL_SHIM_H__
#define EMP_TOOL_SHIM_H__

// Minimal emp-tool shim for portable VOLE (no AES-NI/SSE required)
// Provides: block, PRG, Hash, ThreadPool, NetIO, OTPre

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <future>
#include <memory>
#include <vector>
#include <algorithm>
#include <functional>

extern "C" {
#include "blake3.h"
}

namespace emp {

// Party constants
const int ALICE = 1;
const int BOB = 2;

// Mersenne prime 2^61 - 1
const uint64_t PR = 2305843009213693951ULL;
const uint64_t pr = PR;

inline void error(const char* msg) {
    fprintf(stderr, "Error: %s\n", msg);
    abort();
}

//=============================================================================
// Block type (128-bit)
//=============================================================================

struct block {
    uint64_t data[2];

    block() : data{0, 0} {}
    block(uint64_t high, uint64_t low) : data{low, high} {}

    block operator^(const block& o) const {
        return block(data[1] ^ o.data[1], data[0] ^ o.data[0]);
    }
    block& operator^=(const block& o) {
        data[0] ^= o.data[0]; data[1] ^= o.data[1]; return *this;
    }
    block operator&(const block& o) const {
        return block(data[1] & o.data[1], data[0] & o.data[0]);
    }
    bool operator==(const block& o) const {
        return data[0] == o.data[0] && data[1] == o.data[1];
    }

    // Allow cast from __uint128_t
    block(__uint128_t v) {
        data[0] = (uint64_t)v;
        data[1] = (uint64_t)(v >> 64);
    }
    operator __uint128_t() const {
        return ((__uint128_t)data[1] << 64) | data[0];
    }
};

const block zero_block = block(0, 0);

inline block makeBlock(uint64_t high, uint64_t low) {
    return block(high, low);
}

inline bool getLSB(const block& b) { return b.data[0] & 1; }

inline bool cmpBlock(const block* a, const block* b, int n) {
    for (int i = 0; i < n; i++)
        if (!(a[i] == b[i])) return false;
    return true;
}

inline uint64_t _mm_extract_epi64(const block& b, int idx) {
    return b.data[idx];
}

inline block _mm_set_epi64x(uint64_t hi, uint64_t lo) {
    return block(hi, lo);
}

// Portable SSE-style operations
inline block _mm_add_epi64(const block& a, const block& b) {
    return block(a.data[1] + b.data[1], a.data[0] + b.data[0]);
}

inline block _mm_sub_epi64(const block& a, const block& b) {
    return block(a.data[1] - b.data[1], a.data[0] - b.data[0]);
}

inline block _mm_srli_epi64(const block& a, int imm) {
    return block(a.data[1] >> imm, a.data[0] >> imm);
}

inline block _mm_slli_epi64(const block& a, int imm) {
    return block(a.data[1] << imm, a.data[0] << imm);
}

inline block _mm_andnot_si128(const block& a, const block& b) {
    return block((~a.data[1]) & b.data[1], (~a.data[0]) & b.data[0]);
}

inline block _mm_cmpgt_epi64(const block& a, const block& b) {
    uint64_t r0 = ((int64_t)a.data[0] > (int64_t)b.data[0]) ? ~0ULL : 0ULL;
    uint64_t r1 = ((int64_t)a.data[1] > (int64_t)b.data[1]) ? ~0ULL : 0ULL;
    return block(r1, r0);
}

// vec_mod for portable modular reduction
inline block vec_partial_mod(block i) {
    block prs_local = makeBlock(PR, PR);
    block cmp = _mm_cmpgt_epi64(prs_local, i);
    block mask = _mm_andnot_si128(cmp, prs_local);
    return _mm_sub_epi64(i, mask);
}

inline block vec_mod(block i) {
    block prs_local = makeBlock(PR, PR);
    i = _mm_add_epi64(i & prs_local, _mm_srli_epi64(i, 61));
    return vec_partial_mod(i);
}

//=============================================================================
// Modular arithmetic
//=============================================================================

inline uint64_t mod(uint64_t x) {
    uint64_t i = (x & PR) + (x >> 61);
    return (i >= PR) ? i - PR : i;
}

template <typename T>
T mod(T k, T pv) {
    T i = (k & pv) + (k >> 61);
    return (i >= pv) ? i - pv : i;
}

// Overload for __uint128_t with uint64_t prime
inline __uint128_t mod(__uint128_t k, uint64_t pv) {
    __uint128_t i = (k & pv) + (k >> 61);
    return (i >= pv) ? i - pv : i;
}

inline uint64_t add_mod(uint64_t a, uint64_t b) {
    uint64_t res = a + b;
    return (res >= PR) ? (res - PR) : res;
}

inline uint64_t mult_mod(uint64_t a, uint64_t b) {
    __uint128_t r = (__uint128_t)a * b;
    uint64_t lo = (uint64_t)(r & PR);
    uint64_t hi = (uint64_t)(r >> 61);
    return mod(lo + hi);
}

// extract_fp is defined in utility.h

template <typename T>
void uni_hash_coeff_gen(T *coeff, T seed, int sz) {
    coeff[0] = seed;
    for (int i = 1; i < sz; ++i)
        coeff[i] = mult_mod(coeff[i - 1], seed);
}

template <typename T>
T vector_inn_prdt_sum_red(const T *a, const T *b, int sz) {
    T res = 0;
    for (int i = 0; i < sz; ++i)
        res = add_mod(res, mult_mod(a[i], b[i]));
    return res;
}

//=============================================================================
// BLAKE3-based PRG
//=============================================================================

class PRG {
    uint64_t counter;
    uint8_t buffer[64];
    int buffer_pos;
public:
    PRG(const block* seed = nullptr) : counter(0), buffer_pos(64) {
        if (seed) memcpy(&counter, seed, sizeof(uint64_t));
        else {
            std::random_device rd;
            counter = ((uint64_t)rd() << 32) | rd();
        }
    }

    void reseed(const block* seed) {
        memcpy(&counter, seed, sizeof(uint64_t));
        buffer_pos = 64;
    }

    void random_data(void* data, int nbytes) {
        uint8_t* out = (uint8_t*)data;
        while (nbytes > 0) {
            if (buffer_pos >= 64) {
                blake3_hasher h;
                blake3_hasher_init(&h);
                blake3_hasher_update(&h, &counter, sizeof(counter));
                blake3_hasher_finalize(&h, buffer, 64);
                counter++;
                buffer_pos = 0;
            }
            int copy = std::min(nbytes, 64 - buffer_pos);
            memcpy(out, buffer + buffer_pos, copy);
            out += copy;
            buffer_pos += copy;
            nbytes -= copy;
        }
    }

    void random_block(block* data, int nblocks = 1) {
        random_data(data, nblocks * sizeof(block));
    }

    void random_bool(bool* data, int n) {
        uint8_t* tmp = new uint8_t[n];
        random_data(tmp, n);
        for (int i = 0; i < n; i++)
            data[i] = tmp[i] & 1;
        delete[] tmp;
    }
};

//=============================================================================
// BLAKE3-based Hash
//=============================================================================

class Hash {
    blake3_hasher hasher;
public:
    Hash() { blake3_hasher_init(&hasher); }

    void put(const void* data, int nbytes) {
        blake3_hasher_update(&hasher, data, nbytes);
    }

    void put_block(const block* blk, int nblocks = 1) {
        put(blk, nblocks * sizeof(block));
    }

    void digest(void* output, int nbytes = 32) {
        blake3_hasher_finalize(&hasher, (uint8_t*)output, nbytes);
    }

    void reset() { blake3_hasher_init(&hasher); }

    block hash_for_block(const void* data, int nbytes) {
        blake3_hasher h;
        blake3_hasher_init(&h);
        blake3_hasher_update(&h, data, nbytes);
        block out;
        blake3_hasher_finalize(&h, (uint8_t*)&out, sizeof(block));
        return out;
    }
};

//=============================================================================
// ThreadPool (single-threaded stub)
//=============================================================================

class ThreadPool {
public:
    ThreadPool(int threads = 1) { (void)threads; }

    template<class F>
    auto enqueue(F&& f) -> std::future<typename std::result_of<F()>::type> {
        using return_type = typename std::result_of<F()>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
        std::future<return_type> res = task->get_future();
        (*task)();
        return res;
    }

    int size() const { return 1; }
};

//=============================================================================
// GGM tree expansion (BLAKE3-based TwoKeyPRP)
//=============================================================================

class TwoKeyPRP {
public:
    TwoKeyPRP(block s0 = zero_block, block s1 = zero_block) {
        (void)s0; (void)s1;
    }

    void node_expand_1to2(block *children, block parent) {
        uint8_t input[17], output[32];
        memcpy(input + 1, &parent, 16);
        blake3_hasher h;

        input[0] = 0;
        blake3_hasher_init(&h);
        blake3_hasher_update(&h, input, 17);
        blake3_hasher_finalize(&h, output, 32);
        for (int i = 0; i < 16; i++) output[i] ^= input[i + 1];
        memcpy(&children[0], output, 16);

        input[0] = 1;
        blake3_hasher_init(&h);
        blake3_hasher_update(&h, input, 17);
        blake3_hasher_finalize(&h, output, 32);
        for (int i = 0; i < 16; i++) output[i] ^= input[i + 1];
        memcpy(&children[1], output, 16);
    }

    void node_expand_2to4(block *children, block *parent) {
        node_expand_1to2(children, parent[0]);
        node_expand_1to2(children + 2, parent[1]);
    }
};

//=============================================================================
// NetIO (TCP sockets) or WebSocketIO (WASM)
//=============================================================================

} // namespace emp

#ifdef __EMSCRIPTEN__
// WebSocket-based IO for WASM builds
#include <emscripten.h>
#include <emscripten/websocket.h>

namespace emp {

class NetIO {
    EMSCRIPTEN_WEBSOCKET_T ws;
    std::vector<uint8_t> recv_buffer;
    bool connected;
    bool error_occurred;
    const char* ws_url;

public:
    size_t bytes_sent = 0;
    size_t bytes_recv = 0;

    // Client constructor - address is WebSocket URL (e.g., "ws://localhost:8080")
    NetIO(const char* address, int port) : ws(0), connected(false), error_occurred(false) {
        if (address == nullptr) {
            // Server mode not supported in WASM
            error("WASM NetIO does not support server mode");
            return;
        }

        // Build WebSocket URL
        static char url_buf[256];
        snprintf(url_buf, sizeof(url_buf), "ws://%s:%d", address, port);
        ws_url = url_buf;

        printf("Connecting to %s...\n", ws_url);

        EmscriptenWebSocketCreateAttributes attr;
        emscripten_websocket_init_create_attributes(&attr);
        attr.url = ws_url;
        attr.protocols = nullptr;

        ws = emscripten_websocket_new(&attr);
        if (ws <= 0) {
            error("Failed to create WebSocket");
            return;
        }

        emscripten_websocket_set_onopen_callback(ws, this, on_open);
        emscripten_websocket_set_onmessage_callback(ws, this, on_message);
        emscripten_websocket_set_onerror_callback(ws, this, on_error);
        emscripten_websocket_set_onclose_callback(ws, this, on_close);

        // Wait for connection
        int timeout = 10000;
        while (!connected && !error_occurred && timeout > 0) {
            emscripten_sleep(50);
            timeout -= 50;
        }

        if (!connected) {
            error("WebSocket connection timeout");
            return;
        }
        printf("connected\n");
    }

    ~NetIO() {
        if (ws > 0) {
            emscripten_websocket_close(ws, 1000, "done");
            emscripten_websocket_delete(ws);
        }
    }

    void send_data(const void* data, int len) {
        if (!connected || ws <= 0) return;
        emscripten_websocket_send_binary(ws, (void*)data, len);
        bytes_sent += len;
    }

    void recv_data(void* data, int len) {
        int timeout = 60000;  // 60 second timeout
        while ((int)recv_buffer.size() < len && timeout > 0) {
            if (error_occurred) {
                error("WebSocket error during recv");
                return;
            }
            emscripten_sleep(10);
            timeout -= 10;
        }

        if ((int)recv_buffer.size() < len) {
            fprintf(stderr, "Receive timeout (got %zu, need %d)\n", recv_buffer.size(), len);
            error("WebSocket receive timeout");
            return;
        }

        memcpy(data, recv_buffer.data(), len);
        recv_buffer.erase(recv_buffer.begin(), recv_buffer.begin() + len);
        bytes_recv += len;
    }

    void flush() {}

    void print_stats() {
        printf("Network: sent=%zu bytes, recv=%zu bytes\n", bytes_sent, bytes_recv);
    }

private:
    static EM_BOOL on_open(int, const EmscriptenWebSocketOpenEvent*, void* ud) {
        ((NetIO*)ud)->connected = true;
        return EM_TRUE;
    }
    static EM_BOOL on_message(int, const EmscriptenWebSocketMessageEvent* ev, void* ud) {
        NetIO* io = (NetIO*)ud;
        if (!ev->isText && ev->numBytes > 0) {
            io->recv_buffer.insert(io->recv_buffer.end(), ev->data, ev->data + ev->numBytes);
        }
        return EM_TRUE;
    }
    static EM_BOOL on_error(int, const EmscriptenWebSocketErrorEvent*, void* ud) {
        fprintf(stderr, "WebSocket error\n");
        ((NetIO*)ud)->error_occurred = true;
        return EM_TRUE;
    }
    static EM_BOOL on_close(int, const EmscriptenWebSocketCloseEvent*, void* ud) {
        ((NetIO*)ud)->connected = false;
        return EM_TRUE;
    }
};

} // namespace emp

#else
// Native TCP sockets
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace emp {

class NetIO {
    int sock, consock;
    bool is_server;
public:
    size_t bytes_sent = 0;
    size_t bytes_recv = 0;

    NetIO(const char* address, int port) {
        is_server = (address == nullptr);
        consock = -1;
        sock = socket(AF_INET, SOCK_STREAM, 0);

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (is_server) {
            addr.sin_addr.s_addr = INADDR_ANY;
            int opt = 1;
            setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            bind(sock, (struct sockaddr*)&addr, sizeof(addr));
            listen(sock, 1);
            consock = accept(sock, nullptr, nullptr);
            printf("connected\n");
        } else {
            addr.sin_addr.s_addr = inet_addr(address);
            while (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
                usleep(100000);
            consock = sock;
            printf("connected\n");
        }
    }

    ~NetIO() {
        if (consock >= 0 && consock != sock) close(consock);
        if (sock >= 0) close(sock);
    }

    void send_data(const void* data, int len) {
        int sent = 0;
        while (sent < len) {
            int r = send(consock, (char*)data + sent, len - sent, 0);
            if (r > 0) sent += r;
        }
        bytes_sent += len;
    }

    void recv_data(void* data, int len) {
        int recvd = 0;
        while (recvd < len) {
            int r = recv(consock, (char*)data + recvd, len - recvd, 0);
            if (r > 0) recvd += r;
        }
        bytes_recv += len;
    }

    void flush() {}

    void print_stats() {
        printf("\n--- Network Statistics ---\n");
        printf("Bytes sent:     %zu (%.2f MB)\n", bytes_sent, bytes_sent / 1048576.0);
        printf("Bytes received: %zu (%.2f MB)\n", bytes_recv, bytes_recv / 1048576.0);
    }
};

} // namespace emp
#endif

//=============================================================================
// OTPre (OT Preprocessing)
//=============================================================================

namespace emp {

template <typename IO>
class OTPre {
public:
    IO* io;
    block* pre_data = nullptr;
    bool* bits = nullptr;
    int n, length, count;
    block Delta;

    OTPre(IO* io, int length, int times) : io(io), length(length) {
        n = length * times;
        pre_data = new block[2 * n];
        bits = new bool[n];
        count = 0;
    }

    ~OTPre() {
        delete[] pre_data;
        delete[] bits;
    }

    void send_pre(block* data, block in_Delta) {
        Delta = in_Delta;
        Hash hash;
        for (int i = 0; i < n; ++i) {
            hash.reset();
            hash.put(&i, sizeof(i));
            hash.put(&data[i], sizeof(block));
            hash.digest(&pre_data[i], sizeof(block));
        }
        for (int i = 0; i < n; ++i) {
            block tmp = data[i] ^ Delta;
            hash.reset();
            hash.put(&i, sizeof(i));
            hash.put(&tmp, sizeof(block));
            hash.digest(&pre_data[n + i], sizeof(block));
        }
    }

    void recv_pre(block* data, bool* b) {
        memcpy(bits, b, n);
        Hash hash;
        for (int i = 0; i < n; ++i) {
            hash.reset();
            hash.put(&i, sizeof(i));
            hash.put(&data[i], sizeof(block));
            hash.digest(&pre_data[i], sizeof(block));
        }
    }

    void choices_sender() {
        io->recv_data(bits + count, length);
        count += length;
    }

    void choices_recver(bool* b) {
        for (int i = 0; i < length; ++i)
            bits[count + i] = (b[i] != bits[count + i]);
        io->send_data(bits + count, length);
        count += length;
    }

    void reset() { count = 0; }

    void send(const block* m0, const block* m1, int len, IO* io2, int s) {
        block pad[2];
        int k = s * length;
        for (int i = 0; i < len; ++i) {
            pad[0] = m0[i] ^ pre_data[bits[k] ? k + n : k];
            pad[1] = m1[i] ^ pre_data[bits[k] ? k : k + n];
            ++k;
            io2->send_data(pad, sizeof(pad));
        }
    }

    void recv(block* data, bool* b, int len, IO* io2, int s) {
        int k = s * length;
        block pad[2];
        for (int i = 0; i < len; ++i) {
            io2->recv_data(pad, sizeof(pad));
            // b[i] is the receiver's actual choice - use it to select pad
            data[i] = pre_data[k] ^ pad[b[i] ? 1 : 0];
            ++k;
        }
    }
};

} // namespace emp

#endif // EMP_TOOL_SHIM_H__
