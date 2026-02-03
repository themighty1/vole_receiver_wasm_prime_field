#ifndef SPFSS_RECVER_FP_BLAKE3_H__
#define SPFSS_RECVER_FP_BLAKE3_H__

// SPFSS Receiver using BLAKE3 for GGM tree expansion (no AES-NI dependency)

#include "emp-zk/emp-vole/utility.h"
#include "emp-zk/emp-vole/twokeyprp_blake3.h"
// Note: emp-ot removed - not needed for BLAKE3 version
#include <iostream>

using namespace emp;

template <typename IO> class SpfssRecverFpBlake3 {
public:
  block *ggm_tree, *m;
  __uint128_t *ggm_tree_int;
  bool *b;
  int choice_pos, depth, leave_n;
  IO *io;
  uint64_t share;

  static uint64_t instance_counter;

  SpfssRecverFpBlake3(IO *io, int depth_in) {
    this->io = io;
    this->depth = depth_in;
    this->leave_n = 1 << (depth_in - 1);
    m = new block[depth - 1];
    b = new bool[depth - 1];
    // Initialize choice bits from counter-seeded PRG (avoid slow hardware RNG)
    block seed = makeBlock(0, ++instance_counter);
    PRG prg(&seed);
    prg.random_bool(b, depth - 1);
  }

  ~SpfssRecverFpBlake3() {
    delete[] m;
    delete[] b;
  }

  int get_index() {
    choice_pos = 0;
    for (int i = 0; i < depth - 1; ++i) {
      choice_pos <<= 1;
      if (!b[i])
        choice_pos += 1;
    }
    return choice_pos;
  }

  template <typename OT> void recv(OT *ot, IO *io2, int s) {
    ot->recv(m, b, depth - 1, io2, s);
    io2->recv_data(&share, sizeof(uint64_t));
  }

  void compute(__uint128_t *ggm_tree_mem, __uint128_t delta2) {
    ggm_tree_int = ggm_tree_mem;
    this->ggm_tree = (block *)ggm_tree_mem;
    ggm_tree_reconstruction(b, m);

    ggm_tree[choice_pos] = zero_block;
    uint64_t nodes_sum = (uint64_t)0;
    for (int i = 0; i < leave_n; ++i) {
      extract_fp(ggm_tree_mem[i]);
      nodes_sum = add_mod(nodes_sum, (uint64_t)ggm_tree_mem[i]);
    }
    nodes_sum = add_mod(share, nodes_sum);
    nodes_sum = PR - nodes_sum;
    ggm_tree_mem[choice_pos] =
        add_mod(_mm_extract_epi64((block)delta2, 0), nodes_sum);
  }

  // Reconstruct GGM tree using BLAKE3-based PRG
  void ggm_tree_reconstruction(bool *b, block *m) {
    int to_fill_idx = 0;
    TwoKeyPRP_Blake3 prp(zero_block, makeBlock(0, 1));
    for (int i = 1; i < depth; ++i) {
      to_fill_idx = to_fill_idx * 2;
      ggm_tree[to_fill_idx] = ggm_tree[to_fill_idx + 1] = zero_block;
      if (b[i - 1] == false) {
        layer_recover(i, 0, to_fill_idx, m[i - 1], &prp);
        to_fill_idx += 1;
      } else
        layer_recover(i, 1, to_fill_idx + 1, m[i - 1], &prp);
    }
  }

  void layer_recover(int depth, int lr, int to_fill_idx, block sum,
                     TwoKeyPRP_Blake3 *prp) {
    int layer_start = 0;
    int item_n = 1 << depth;
    block nodes_sum = zero_block;
    int lr_start = lr == 0 ? layer_start : (layer_start + 1);

    for (int i = lr_start; i < item_n; i += 2)
      nodes_sum = nodes_sum ^ ggm_tree[i];
    ggm_tree[to_fill_idx] = nodes_sum ^ sum;
    if (depth == this->depth - 1)
      return;
    for (int i = item_n - 2; i >= 0; i -= 2)
      prp->node_expand_2to4(&ggm_tree[i * 2], &ggm_tree[i]);
  }

  void consistency_check(IO *io2, __uint128_t z, __uint128_t beta) {
    __uint128_t *chi = new __uint128_t[leave_n];
    Hash hash;
    __uint128_t digest =
        (__uint128_t)hash.hash_for_block(&share, sizeof(uint64_t));
    digest = mod(_mm_extract_epi64((block)digest, 0));
    uni_hash_coeff_gen(chi, digest, leave_n);

    __uint128_t tmp = mod(chi[choice_pos] * (beta >> 64), pr);
    __uint128_t x_star = pr - (z >> 64);
    x_star = mod(x_star + tmp, pr);
    io2->send_data(&x_star, sizeof(__uint128_t));
    io2->flush();

    __uint128_t W =
        vector_inn_prdt_sum_red(chi, (__uint128_t *)ggm_tree, leave_n);
    tmp = pr - ((__uint128_t)z & 0xFFFFFFFFFFFFFFFFLL);
    W = mod(W + tmp, pr);

    __uint128_t V;
    io2->recv_data(&V, sizeof(__uint128_t));

    if (W != V) {
      std::cout << "SPFSS consistency check fails" << std::endl;
      abort();
    }

    uint64_t tmp2 = (uint64_t)(beta >> 64);
    ggm_tree_int[choice_pos] =
        ((__uint128_t)tmp2 << 64) ^ ggm_tree_int[choice_pos];

    delete[] chi;
  }

  void consistency_check_msg_gen(__uint128_t &chi_alpha, __uint128_t &W,
                                 IO *io2, __uint128_t beta, block seed) {
    __uint128_t *chi = new __uint128_t[leave_n];
    Hash hash;
    __uint128_t digest =
        mod(_mm_extract_epi64(hash.hash_for_block(&seed, sizeof(block)), 0));
    uni_hash_coeff_gen(chi, digest, leave_n);

    chi_alpha = chi[choice_pos];

    W = vector_inn_prdt_sum_red(chi, (__uint128_t *)ggm_tree, leave_n);

    uint64_t tmp2 = _mm_extract_epi64((block)beta, 1);
    ggm_tree_int[choice_pos] =
        ((__uint128_t)tmp2 << 64) ^ ggm_tree_int[choice_pos];

    delete[] chi;
  }
};

template <typename IO>
uint64_t SpfssRecverFpBlake3<IO>::instance_counter = 0;

#endif // SPFSS_RECVER_FP_BLAKE3_H__
