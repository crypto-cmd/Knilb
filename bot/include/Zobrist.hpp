#pragma once
#include <array>
#include <cstdint>

namespace Knilb::Precomputed::Zobrist {
using ZobristHash= uint64_t;
// 1. A compile-time PRNG (XorShift64*)
class PRNG {
    uint64_t state;

  public:
    // The seed must be non-zero
    constexpr PRNG(uint64_t seed): state(seed) {}

    constexpr uint64_t rand64() {
        state^= state >> 12;
        state^= state << 25;
        state^= state >> 27;
        return state * 0x2545F4914F6CDD1DULL;
    }
};

// 2. The Data Structure containing all our tables
struct ZobristTables {
    std::array<std::array<uint64_t, 64>, 12> pieces{};
    std::array<uint64_t, 16> castling{};
    std::array<uint64_t, 65> enPassant{};
    uint64_t sideToMove{};

    // 3. The constructor that the compiler will run during the build
    constexpr ZobristTables(uint64_t seed) {
        PRNG rng(seed);

        for(int p= 0; p < 12; ++p) {
            for(int s= 0; s < 64; ++s) {
                pieces[p][s]= rng.rand64();
            }
        }

        for(int c= 0; c < 16; ++c) {
            castling[c]= rng.rand64();
        }

        for(int e= 0; e < 65; ++e) {
            enPassant[e]= rng.rand64();
        }

        sideToMove= rng.rand64();
    }
};

// 4. THE MAGIC LINE: We instantiate the struct at compile time.
// We pass a hardcoded seed (e.g., 1070372).
// This guarantees the exact same random numbers every time you build the engine.
constexpr ZobristTables Tables(1070372ULL);
} // namespace Knilb::Precomputed::Zobrist