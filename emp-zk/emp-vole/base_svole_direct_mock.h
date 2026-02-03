#ifndef BASE_VOLE_DIRECT_MOCK_H__
#define BASE_VOLE_DIRECT_MOCK_H__

// Note: emp-tool provided via emp-vole-mock.h
#include "emp-zk/emp-vole/utility.h"

// Direct mock VOLE - NO communication, hardcoded correlations
// Both parties derive same Delta and values from fixed seeds
// WARNING: NOT SECURE - for benchmarking only!

template <typename IO> class Base_svole_direct_mock {
public:
  int party;
  IO *io;
  __uint128_t Delta;
  PRG sync_prg;
  static int64_t total_base_voles;

  // Hardcoded Delta derived from fixed seed (same for both parties)
  static __uint128_t get_hardcoded_delta() {
    PRG delta_prg;
    block delta_seed = makeBlock(0xDE17AF00DE17AF00ULL, 0x00F017A000F017A0ULL);
    delta_prg.reseed(&delta_seed);
    uint64_t delta_val;
    delta_prg.random_data(&delta_val, sizeof(uint64_t));
    return mod(delta_val);
  }

  // SENDER - Delta parameter ignored, use hardcoded
  Base_svole_direct_mock(int party, IO *io, __uint128_t /*Delta*/) {
    this->party = party;
    this->io = io;
    this->Delta = get_hardcoded_delta();  // Use hardcoded, ignore parameter

    block seed = makeBlock(0x12345678ABCDEF00ULL, 0xFEDCBA9876543210ULL);
    sync_prg.reseed(&seed);
  }

  // RECEIVER - derives same Delta from hardcoded seed
  Base_svole_direct_mock(int party, IO *io) {
    this->party = party;
    this->io = io;
    this->Delta = get_hardcoded_delta();  // Same as sender

    block seed = makeBlock(0x12345678ABCDEF00ULL, 0xFEDCBA9876543210ULL);
    sync_prg.reseed(&seed);
  }

  ~Base_svole_direct_mock() {}

  // Sender: get y values where z = x·Δ + y
  void triple_gen_send(__uint128_t *share, int size) {
    total_base_voles += size;
    for (int i = 0; i < size; ++i) {
      uint64_t x, y;
      sync_prg.random_data(&x, sizeof(uint64_t));
      sync_prg.random_data(&y, sizeof(uint64_t));
      x = mod(x);
      y = mod(y);
      share[i] = y; // Sender gets y
    }
  }

  // Receiver: get (x, z) where z = x·Δ + y
  void triple_gen_recv(__uint128_t *share, int size) {
    total_base_voles += size;
    for (int i = 0; i < size; ++i) {
      uint64_t x, y;
      sync_prg.random_data(&x, sizeof(uint64_t));
      sync_prg.random_data(&y, sizeof(uint64_t));
      x = mod(x);
      y = mod(y);

      // z = x·Δ + y (correct VOLE correlation)
      uint64_t z = mult_mod(x, (uint64_t)Delta);
      z = add_mod(z, y);

      // Pack as (x, z) in high/low 64 bits
      share[i] = ((__uint128_t)x << 64) | z;
    }
  }
};

template <typename IO>
int64_t Base_svole_direct_mock<IO>::total_base_voles = 0;

#endif // BASE_VOLE_DIRECT_MOCK_H__
