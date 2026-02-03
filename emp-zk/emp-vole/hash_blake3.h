#ifndef HASH_BLAKE3_H__
#define HASH_BLAKE3_H__

// Disable SIMD for portable WASM compatibility
#ifndef BLAKE3_NO_SSE2
#define BLAKE3_NO_SSE2
#endif
#ifndef BLAKE3_NO_SSE41
#define BLAKE3_NO_SSE41
#endif
#ifndef BLAKE3_NO_AVX2
#define BLAKE3_NO_AVX2
#endif
#ifndef BLAKE3_NO_AVX512
#define BLAKE3_NO_AVX512
#endif
#ifndef BLAKE3_NO_NEON
#define BLAKE3_NO_NEON
#endif

#include "emp-zk/emp-vole/blake3.h"
#include <cstring>
#include <cstdint>
#include <emmintrin.h>  // For __m128i, _mm_loadu_si128

// Forward declare block type (will be defined by emp-tool later)
namespace emp {
    typedef __m128i block;
}

namespace emp {

// Drop-in replacement for emp::Hash using BLAKE3 instead of SHA256
// No OpenSSL dependency, portable (no SIMD)
class Hash {
public:
    blake3_hasher hasher;
    static const int DIGEST_SIZE = 32;

    Hash() {
        blake3_hasher_init(&hasher);
    }

    ~Hash() {}

    void put(const void *data, int nbyte) {
        blake3_hasher_update(&hasher, data, nbyte);
    }

    void put_block(const block *blk, int nblock = 1) {
        put(blk, sizeof(block) * nblock);
    }

    void digest(void *a) {
        blake3_hasher_finalize(&hasher, (uint8_t *)a, DIGEST_SIZE);
        reset();
    }

    void reset() {
        blake3_hasher_init(&hasher);
    }

    static void hash_once(void *dgst, const void *data, int nbyte) {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, data, nbyte);
        blake3_hasher_finalize(&hasher, (uint8_t *)dgst, DIGEST_SIZE);
    }

    #ifdef __x86_64__
    __attribute__((target("sse2")))
    #endif
    static block hash_for_block(const void *data, int nbyte) {
        uint8_t digest[DIGEST_SIZE];
        hash_once(digest, data, nbyte);
        return _mm_loadu_si128((__m128i *)digest);
    }

    // KDF function - compatible with original Hash::KDF
    // Note: Original uses EC Point, but in VOLE we only use simple KDF
    static block KDF(const void *data, size_t len, uint64_t id = 1) {
        blake3_hasher hasher;
        blake3_hasher_init(&hasher);
        blake3_hasher_update(&hasher, data, len);
        blake3_hasher_update(&hasher, &id, 8);
        uint8_t digest[DIGEST_SIZE];
        blake3_hasher_finalize(&hasher, digest, DIGEST_SIZE);
        return _mm_loadu_si128((__m128i *)digest);
    }
};

} // namespace emp

#endif // HASH_BLAKE3_H__
