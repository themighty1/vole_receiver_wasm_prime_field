#ifndef EMP_OTCO_MOCK_H__
#define EMP_OTCO_MOCK_H__

#include <emp-tool/emp-tool.h>

namespace emp {

/*
 * Mock OTCO - Drop-in replacement for Chou-Orlandi OT
 * Uses synchronized PRG instead of elliptic curves
 * WARNING: NOT SECURE - for benchmarking only!
 *
 * Same interface as OTCO so it can be used directly in COPE and IKNP
 */

template<typename IO>
class OTCOMock {
public:
    IO* io;
    PRG sync_prg;

    OTCOMock(IO* io, void* unused = nullptr) : io(io) {
        // Synchronized seed - both parties generate same randomness
        block seed = makeBlock(0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL);
        sync_prg.reseed(&seed);
    }

    ~OTCOMock() {}

    void send(const block* data0, const block* data1, int64_t length) {
        // Generate synchronized keys
        block* keys = new block[length];
        sync_prg.random_block(keys, length);

        // Receive choice bits from receiver (simulated communication)
        bool* choices = new bool[length];
        io->recv_data(choices, length);

        // Send masked values
        block res[2];
        for (int64_t i = 0; i < length; ++i) {
            // Use key to mask both values
            // In real OT, sender doesn't know which one receiver gets
            // Here we just simulate the data transfer pattern
            res[0] = keys[i] ^ data0[i];
            res[1] = keys[i] ^ data1[i];
            io->send_data(res, 2 * sizeof(block));
        }
        io->flush();

        delete[] keys;
        delete[] choices;
    }

    void recv(block* data, const bool* b, int64_t length) {
        // Generate synchronized keys (same as sender)
        block* keys = new block[length];
        sync_prg.random_block(keys, length);

        // Send choice bits to sender
        io->send_data(b, length);
        io->flush();

        // Receive masked values and unmask
        block res[2];
        for (int64_t i = 0; i < length; ++i) {
            io->recv_data(res, 2 * sizeof(block));
            // Unmask the chosen value
            data[i] = keys[i] ^ (b[i] ? res[1] : res[0]);
        }

        delete[] keys;
    }
};

// Alias to replace OTCO with mock
#ifdef USE_MOCK_OT
#define OTCO OTCOMock
#endif

} // namespace emp

#endif // EMP_OTCO_MOCK_H__
