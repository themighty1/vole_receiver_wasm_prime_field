#ifndef _PRE_OT_BLAKE3__
#define _PRE_OT_BLAKE3__

// BLAKE3-based OTPre - no AES dependency
// Uses BLAKE3 hash instead of CCRH for correlation robustness

#include <cstring>
#include <vector>

namespace emp {

// Forward declare Hash (provided by hash_blake3.h)
class Hash;

template <typename IO> class OTPre {
public:
  IO *io;
  block *pre_data = nullptr;
  bool *bits = nullptr;
  int n;
  std::vector<block *> pointers;
  std::vector<const bool *> choices;
  std::vector<const block *> pointers0;
  std::vector<const block *> pointers1;

  int length, count;
  block Delta;

  OTPre(IO *io, int length, int times) {
    this->io = io;
    this->length = length;
    n = length * times;
    pre_data = new block[2 * n];
    bits = new bool[n];
    count = 0;
  }

  ~OTPre() {
    if (pre_data != nullptr)
      delete[] pre_data;
    if (bits != nullptr)
      delete[] bits;
  }

  // Hash n blocks using BLAKE3 (replaces CCRH.Hn)
  void hash_blocks(block *out, const block *in, int start, int num) {
    Hash hash;
    for (int i = 0; i < num; ++i) {
      hash.reset();
      // Include index for domain separation
      uint64_t idx = start + i;
      hash.put(&idx, sizeof(idx));
      hash.put(&in[i], sizeof(block));
      hash.digest(out + i);
    }
  }

  // Hash with additional XOR data (for sender path)
  void hash_blocks_with_delta(block *out, const block *in, int start, int num, block *extra) {
    Hash hash;
    for (int i = 0; i < num; ++i) {
      hash.reset();
      uint64_t idx = start + i;
      hash.put(&idx, sizeof(idx));
      hash.put(&in[i], sizeof(block));
      hash.digest(out + i);
    }
  }

  void send_pre(block *data, block in_Delta) {
    Delta = in_Delta;
    // Hash input data -> pre_data[0..n)
    hash_blocks(pre_data, data, 0, n);
    // XOR with Delta and hash -> pre_data[n..2n)
    xorBlocks_arr(pre_data + n, data, Delta, n);
    hash_blocks(pre_data + n, pre_data + n, 0, n);
  }

  void recv_pre(block *data, bool *b) {
    memcpy(bits, b, n);
    hash_blocks(pre_data, data, 0, n);
  }

  void recv_pre(block *data) {
    for (int i = 0; i < n; ++i)
      bits[i] = getLSB(data[i]);
    hash_blocks(pre_data, data, 0, n);
  }

  void choices_sender() {
    io->recv_data(bits + count, length);
    count += length;
  }

  void choices_recver(const bool *b) {
    for (int i = 0; i < length; ++i) {
      bits[count + i] = (b[i] != bits[count + i]);
    }
    io->send_data(bits + count, length);
    count += length;
  }

  void reset() { count = 0; }

  void send(const block *m0, const block *m1, int length, IO *io2, int s) {
    block pad[2];
    int k = s * length;
    for (int i = 0; i < length; ++i) {
      if (!bits[k]) {
        pad[0] = m0[i] ^ pre_data[k];
        pad[1] = m1[i] ^ pre_data[k + n];
      } else {
        pad[0] = m0[i] ^ pre_data[k + n];
        pad[1] = m1[i] ^ pre_data[k];
      }
      ++k;
      io2->send_block(pad, 2);
    }
  }

  void recv(block *data, const bool *b, int length, IO *io2, int s) {
    int k = s * length;
    block pad[2];
    for (int i = 0; i < length; ++i) {
      io2->recv_block(pad, 2);
      int ind = b[i] ? 1 : 0;
      data[i] = pre_data[k] ^ pad[ind];
      ++k;
    }
  }
};

} // namespace emp
#endif // _PRE_OT_BLAKE3__
