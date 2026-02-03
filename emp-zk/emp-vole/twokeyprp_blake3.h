#ifndef EMP_TWOKEYPRP_BLAKE3_H__
#define EMP_TWOKEYPRP_BLAKE3_H__

#include "emp-zk/emp-vole/blake3.h"
#include <cstring>

namespace emp {

// BLAKE3-based GGM tree node expansion
// Drop-in replacement for TwoKeyPRP (AES-based)
// Uses counter-based construction: child = H(counter || parent) XOR parent

class TwoKeyPRP_Blake3 {
public:
  // Constructor takes two blocks but we use simple counter-based expansion
  // seed0/seed1 are ignored - we use fixed counters 0 and 1
  TwoKeyPRP_Blake3(block s0, block s1) {
    (void)s0; (void)s1;  // Unused - we use counter-based construction
  }

  // Expand 1 parent node to 2 children using counter-based PRG
  // child[0] = H(0 || parent) XOR parent
  // child[1] = H(1 || parent) XOR parent
  void node_expand_1to2(block *children, block parent) {
    uint8_t input[17];  // 1 byte counter + 16 byte parent
    uint8_t output[32];
    uint8_t parent_bytes[16];
    memcpy(parent_bytes, &parent, 16);
    blake3_hasher hasher;

    // Left child: H(0 || parent) XOR parent
    input[0] = 0;
    memcpy(input + 1, parent_bytes, 16);
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, input, 17);
    blake3_hasher_finalize(&hasher, output, 32);
    for (int i = 0; i < 16; i++) output[i] ^= parent_bytes[i];
    memcpy(&children[0], output, 16);

    // Right child: H(1 || parent) XOR parent
    input[0] = 1;
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, input, 17);
    blake3_hasher_finalize(&hasher, output, 32);
    for (int i = 0; i < 16; i++) output[i] ^= parent_bytes[i];
    memcpy(&children[1], output, 16);
  }

  // Expand 2 parent nodes to 4 children
  void node_expand_2to4(block *children, block *parent) {
    uint8_t input[17];
    uint8_t output[32];
    uint8_t parent0_bytes[16], parent1_bytes[16];
    memcpy(parent0_bytes, &parent[0], 16);
    memcpy(parent1_bytes, &parent[1], 16);
    blake3_hasher hasher;

    // children[0] = H(0 || parent[0]) XOR parent[0]
    input[0] = 0;
    memcpy(input + 1, parent0_bytes, 16);
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, input, 17);
    blake3_hasher_finalize(&hasher, output, 32);
    for (int i = 0; i < 16; i++) output[i] ^= parent0_bytes[i];
    memcpy(&children[0], output, 16);

    // children[1] = H(1 || parent[0]) XOR parent[0]
    input[0] = 1;
    memcpy(input + 1, parent0_bytes, 16);
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, input, 17);
    blake3_hasher_finalize(&hasher, output, 32);
    for (int i = 0; i < 16; i++) output[i] ^= parent0_bytes[i];
    memcpy(&children[1], output, 16);

    // children[2] = H(0 || parent[1]) XOR parent[1]
    input[0] = 0;
    memcpy(input + 1, parent1_bytes, 16);
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, input, 17);
    blake3_hasher_finalize(&hasher, output, 32);
    for (int i = 0; i < 16; i++) output[i] ^= parent1_bytes[i];
    memcpy(&children[2], output, 16);

    // children[3] = H(1 || parent[1]) XOR parent[1]
    input[0] = 1;
    memcpy(input + 1, parent1_bytes, 16);
    blake3_hasher_init(&hasher);
    blake3_hasher_update(&hasher, input, 17);
    blake3_hasher_finalize(&hasher, output, 32);
    for (int i = 0; i < 16; i++) output[i] ^= parent1_bytes[i];
    memcpy(&children[3], output, 16);
  }
};

} // namespace emp

#endif // EMP_TWOKEYPRP_BLAKE3_H__
