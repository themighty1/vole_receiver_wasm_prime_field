#ifndef BASE_COT_MOCK_H__
#define BASE_COT_MOCK_H__

// Note: emp-tool and OTPre provided via emp-vole-mock.h
// This file uses preot_blake3.h for OTPre

// Mock BaseCot that doesn't use IKNP/OTCO
// Uses synchronized PRG for benchmarking only
// WARNING: NOT SECURE - for benchmarking only!

template<typename IO>
class BaseCotMock {
public:
    int party;
    block one, minusone;
    block ot_delta;
    IO *io;
    PRG sync_prg;
    bool malicious = false;
    static int64_t total_cots;

    BaseCotMock(int party, IO *io, bool malicious = false) {
        this->party = party;
        this->io = io;
        this->malicious = malicious;
        minusone = makeBlock(0xFFFFFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFFFFELL);
        one = makeBlock(0x0LL, 0x1LL);

        // Use synchronized seed for PRG
        block seed = makeBlock(0x1234567890ABCDEFULL, 0xFEDCBA0987654321ULL);
        sync_prg.reseed(&seed);

        // Generate synchronized Delta (both parties derive same value)
        PRG delta_prg;
        block delta_seed = makeBlock(0xDE17A000DE17A000ULL, 0x000DE17A000DE17AULL);
        delta_prg.reseed(&delta_seed);
        delta_prg.random_block(&ot_delta, 1);
        ot_delta = ot_delta & minusone;
        ot_delta = ot_delta ^ one;
    }

    ~BaseCotMock() {}

    void cot_gen_pre(block deltain) {
        if (this->party == ALICE) {
            this->ot_delta = deltain;
        }
        // Synchronize
        io->flush();
        int dummy = 0;
        if (party == ALICE) {
            io->send_data(&dummy, sizeof(int));
            io->recv_data(&dummy, sizeof(int));
        } else {
            io->recv_data(&dummy, sizeof(int));
            io->send_data(&dummy, sizeof(int));
        }
        io->flush();
    }

    void cot_gen_pre() {
        // Mock: keep the synchronized Delta from constructor
        // (Don't regenerate like real BaseCot does)

        // Synchronize
        io->flush();
        int dummy = 0;
        if (party == ALICE) {
            io->send_data(&dummy, sizeof(int));
            io->recv_data(&dummy, sizeof(int));
        } else {
            io->recv_data(&dummy, sizeof(int));
            io->send_data(&dummy, sizeof(int));
        }
        io->flush();
    }

    // Generate COTs using synchronized PRG
    void cot_gen(block *ot_data, int64_t size, bool *pre_bool = nullptr) {
        total_cots += size;
        // Generate synchronized random data
        sync_prg.random_block(ot_data, size);

        if (this->party == ALICE) {
            for (int64_t i = 0; i < size; ++i)
                ot_data[i] = ot_data[i] & minusone;
        } else {
            block ch[2];
            ch[0] = zero_block;
            ch[1] = makeBlock(0, 1);

            bool *choice = new bool[size];
            if (pre_bool && !malicious) {
                memcpy(choice, pre_bool, size);
            } else {
                PRG prg;
                prg.random_bool(choice, size);
            }

            for (int64_t i = 0; i < size; ++i)
                ot_data[i] = (ot_data[i] & minusone) ^ ch[choice[i]];

            delete[] choice;
        }

        // Simulate communication delay
        io->flush();
    }

    // Generate COTs for OTPre with proper correlation:
    // Alice has: M[i]
    // Bob has: M[i] ^ (choice[i] * Delta)
    void cot_gen(OTPre<IO> *pre_ot, int64_t size, bool *pre_bool = nullptr) {
        total_cots += size;
        block *ot_data = new block[size];
        bool *choice = new bool[size];

        // Generate synchronized M values and choice bits
        sync_prg.random_block(ot_data, size);
        sync_prg.random_bool(choice, size);

        if (this->party == ALICE) {
            // Sender: M[i] with LSB cleared
            for (int64_t i = 0; i < size; ++i)
                ot_data[i] = ot_data[i] & minusone;
            pre_ot->send_pre(ot_data, ot_delta);
        } else {
            // Receiver: M[i] ^ (choice[i] * Delta)
            for (int64_t i = 0; i < size; ++i) {
                ot_data[i] = ot_data[i] & minusone;
                if (choice[i])
                    ot_data[i] = ot_data[i] ^ ot_delta;
            }
            pre_ot->recv_pre(ot_data, choice);
        }

        delete[] choice;
        delete[] ot_data;
        io->flush();
    }
};

template<typename IO>
int64_t BaseCotMock<IO>::total_cots = 0;

#endif // BASE_COT_MOCK_H__
