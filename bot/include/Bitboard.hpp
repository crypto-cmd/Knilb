#pragma once

#include <bit>
#include <cstdint>

namespace Knilb::Representation::Bitboards {
using Bitboard= uint64_t;
constexpr void SetBit(Bitboard& board, int square) {
    board|= (1ULL << square);
}
inline void PopBit(Bitboard& board, int square) {
    board&= ~(1ULL << square);
}
inline bool GetBit(const Bitboard& board, int square) {
    return (board >> square) & 1ULL;
}
inline int CountBits(Bitboard board) {
    return std::popcount(board);
}
inline int GetLSBIndex(Bitboard board) {
    return std::countr_zero(board);
}

// Remove LSB and return its index (The workhorse of move gen loops)
inline int PopLSB(Bitboard& bb) {
    int idx= GetLSBIndex(bb);
    bb&= bb - 1; // Clear LSB
    return idx;
}
inline int GetMSBIndex(Bitboard bb) {
    return 63 - std::countl_zero(bb);
}

inline int GetMSB(Bitboard bb) {
    return 1ULL << GetMSBIndex(bb);
}
inline int GetLSB(Bitboard bb) {
    return 1ULL << GetLSBIndex(bb);
}
constexpr Bitboard FileA= 0x0101010101010101ULL;
constexpr Bitboard FileB= FileA << 1;
constexpr Bitboard FileC= FileA << 2;
constexpr Bitboard FileD= FileA << 3;
constexpr Bitboard FileE= FileA << 4;
constexpr Bitboard FileF= FileA << 5;
constexpr Bitboard FileG= FileA << 6;
constexpr Bitboard FileH= FileA << 7;

constexpr Bitboard Rank1= 0xFFULL;
constexpr Bitboard Rank2= Rank1 << 8;
constexpr Bitboard Rank3= Rank1 << 16;
constexpr Bitboard Rank4= Rank1 << 24;
constexpr Bitboard Rank5= Rank1 << 32;
constexpr Bitboard Rank6= Rank1 << 40;
constexpr Bitboard Rank7= Rank1 << 48;
constexpr Bitboard Rank8= Rank1 << 56;

// Squares f1, g1
constexpr Bitboard WhiteKingSideMask= 0x60ULL;
// Squares b1, c1, d1
constexpr Bitboard WhiteQueenSideMask= 0x0EULL;
// Squares f8, g8
constexpr Bitboard BlackKingSideMask= 0x6000000000000000ULL;
// Squares b8, c8, d8
constexpr Bitboard BlackQueenSideMask= 0x0E00000000000000ULL;

} // namespace Knilb::Representation::Bitboards