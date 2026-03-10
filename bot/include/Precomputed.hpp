#pragma once
#include <array>

namespace Knilb::Precomputed {

namespace Direction {
namespace Index {
enum Type : int {
    NORTH= 0,
    SOUTH= 1,
    EAST= 2,
    WEST= 3,
    NORTH_WEST= 4,
    SOUTH_EAST= 5,
    NORTH_EAST= 6,
    SOUTH_WEST= 7
};
}; // namespace Index
namespace Offset {
inline constexpr int NORTH= 8;
inline constexpr int SOUTH= -8;
inline constexpr int EAST= 1;
inline constexpr int WEST= -1;
inline constexpr int NORTH_WEST= NORTH + WEST;
inline constexpr int SOUTH_EAST= SOUTH + EAST;
inline constexpr int NORTH_EAST= NORTH + EAST;
inline constexpr int SOUTH_WEST= SOUTH + WEST;
}; // namespace Offset
}; // namespace Direction

constexpr int DirectionOffsets[8]= {
    Direction::Offset::NORTH,
    Direction::Offset::SOUTH,
    Direction::Offset::EAST,
    Direction::Offset::WEST,
    Direction::Offset::NORTH_WEST,
    Direction::Offset::SOUTH_EAST,
    Direction::Offset::NORTH_EAST,
    Direction::Offset::SOUTH_WEST};
struct BoardData {
    std::array<std::array<int, 8>, 64> NumSquaresToEdge; // [square][direction]
    std::array<std::array<int, 64>, 64> SquareDistance;  // [square][square] -> King steps
};
constexpr BoardData PrecomputeBoardData() {
    BoardData data{};
    for(int file= 0; file < 8; ++file) {
        for(int rank= 0; rank < 8; ++rank) {
            int square= rank * 8 + file;

            int north= 7 - rank;
            int south= rank;
            int east= 7 - file;
            int west= file;

            // Calculate distance to edges
            data.NumSquaresToEdge[square][Direction::Index::NORTH]= north;
            data.NumSquaresToEdge[square][Direction::Index::SOUTH]= south;
            data.NumSquaresToEdge[square][Direction::Index::EAST]= east;
            data.NumSquaresToEdge[square][Direction::Index::WEST]= west;

            data.NumSquaresToEdge[square][Direction::Index::NORTH_WEST]= north < west ? north : west;
            data.NumSquaresToEdge[square][Direction::Index::SOUTH_EAST]= south < east ? south : east;
            data.NumSquaresToEdge[square][Direction::Index::NORTH_EAST]= north < east ? north : east;
            data.NumSquaresToEdge[square][Direction::Index::SOUTH_WEST]= south < west ? south : west;

            // Calculate distance to all other squares
            for(int square2= 0; square2 < 64; ++square2) {
                int file2= square2 % 8;
                int rank2= square2 / 8;
                int fDist= file > file2 ? file - file2 : file2 - file;               // Absolute file distance
                int rDist= rank > rank2 ? rank - rank2 : rank2 - rank;               // Absolute rank distance
                data.SquareDistance[square][square2]= fDist > rDist ? fDist : rDist; // Chebyshev distance for king
            }
        }
    }
    return data;
}
constexpr BoardData Tables= PrecomputeBoardData();

// Passed pawn masks: [color][square]
#include "Bitboard.hpp"
namespace PassedPawnMask {
using Knilb::Representation::Bitboards::Bitboard;
constexpr Bitboard White[64]= {
    // A1-H1
    0x0101010101010100ULL, 0x0303030303030200ULL, 0x0707070707070400ULL, 0x0F0F0F0F0F0F0800ULL,
    0x1F1F1F1F1F1F1000ULL, 0x3F3F3F3F3F3F2000ULL, 0x7F7F7F7F7F7F4000ULL, 0xFEFEFEFEFEFE8000ULL,
    // A2-H2
    0x0101010101010000ULL, 0x0303030303020000ULL, 0x0707070707040000ULL, 0x0F0F0F0F0F080000ULL,
    0x1F1F1F1F1F100000ULL, 0x3F3F3F3F3F200000ULL, 0x7F7F7F7F7F400000ULL, 0xFEFEFEFEFE800000ULL,
    // A3-H3
    0x0101010101000000ULL, 0x0303030302000000ULL, 0x0707070704000000ULL, 0x0F0F0F0F08000000ULL,
    0x1F1F1F1F10000000ULL, 0x3F3F3F3F20000000ULL, 0x7F7F7F7F40000000ULL, 0xFEFEFEFE80000000ULL,
    // A4-H4
    0x0101010100000000ULL, 0x0303030200000000ULL, 0x0707070400000000ULL, 0x0F0F0F0800000000ULL,
    0x1F1F1F1000000000ULL, 0x3F3F3F2000000000ULL, 0x7F7F7F4000000000ULL, 0xFEFEFE8000000000ULL,
    // A5-H5
    0x0101010000000000ULL, 0x0303020000000000ULL, 0x0707040000000000ULL, 0x0F0F080000000000ULL,
    0x1F1F100000000000ULL, 0x3F3F200000000000ULL, 0x7F7F400000000000ULL, 0xFEFE800000000000ULL,
    // A6-H6
    0x0101000000000000ULL, 0x0302000000000000ULL, 0x0704000000000000ULL, 0x0F08000000000000ULL,
    0x1F10000000000000ULL, 0x3F20000000000000ULL, 0x7F40000000000000ULL, 0xFE80000000000000ULL,
    // A7-H7
    0x0100000000000000ULL, 0x0300000000000000ULL, 0x0700000000000000ULL, 0x0F00000000000000ULL,
    0x1F00000000000000ULL, 0x3F00000000000000ULL, 0x7F00000000000000ULL, 0xFE00000000000000ULL,
    // A8-H8
    0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
    0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL};
constexpr Bitboard Black[64]= {
    // A1-H1
    0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
    0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL, 0x0000000000000000ULL,
    // A2-H2
    0x0000000000000001ULL, 0x0000000000000003ULL, 0x0000000000000007ULL, 0x000000000000000FULL,
    0x000000000000001FULL, 0x000000000000003FULL, 0x000000000000007FULL, 0x00000000000000FEULL,
    // A3-H3
    0x0000000000000101ULL, 0x0000000000000303ULL, 0x0000000000000707ULL, 0x0000000000000F0FULL,
    0x0000000000001F1FULL, 0x0000000000003F3FULL, 0x0000000000007F7FULL, 0x000000000000FEFEULL,
    // A4-H4
    0x0000000000010101ULL, 0x0000000000030303ULL, 0x0000000000070707ULL, 0x00000000000F0F0FULL,
    0x00000000001F1F1FULL, 0x00000000003F3F3FULL, 0x00000000007F7F7FULL, 0x0000000000FEFEFEULL,
    // A5-H5
    0x0000000001010101ULL, 0x0000000003030303ULL, 0x0000000007070707ULL, 0x000000000F0F0F0FULL,
    0x000000001F1F1F1FULL, 0x000000003F3F3F3FULL, 0x000000007F7F7F7FULL, 0x00000000FEFEFEFEULL,
    // A6-H6
    0x0000000101010101ULL, 0x0000000303030303ULL, 0x0000000707070707ULL, 0x0000000F0F0F0F0FULL,
    0x0000001F1F1F1F1FULL, 0x0000003F3F3F3F3FULL, 0x0000007F7F7F7F7FULL, 0x000000FEFEFEFEFEULL,
    // A7-H7
    0x0000010101010101ULL, 0x0000030303030303ULL, 0x0000070707070707ULL, 0x00000F0F0F0F0F0FULL,
    0x00001F1F1F1F1F1FULL, 0x00003F3F3F3F3F3FULL, 0x00007F7F7F7F7F7FULL, 0x0000FEFEFEFEFEFEULL,
    // A8-H8
    0x0001010101010101ULL, 0x0003030303030303ULL, 0x0007070707070707ULL, 0x000F0F0F0F0F0F0FULL,
    0x001F1F1F1F1F1F1FULL, 0x003F3F3F3F3F3F3FULL, 0x007F7F7F7F7F7F7FULL, 0x00FEFEFEFEFEFEFEULL};
} // namespace PassedPawnMask
}; // namespace Knilb::Precomputed