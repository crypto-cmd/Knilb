#pragma once

#include "Coordinate.hpp"
#include "Piece.hpp"
#include <string>
namespace Knilb::Representation::Moves {

using Move= uint16_t; // [from: 6 bits][to: 6 bits][flag: 4 bit]
inline constexpr Move DEFAULT_MOVE= 0;
inline constexpr uint16_t CAPTURE_MASK= 0b0100;
inline constexpr uint16_t PROMOTION_MASK= 0b1000;

namespace MoveFlag {

inline constexpr uint16_t QUIET= 0;            // 0000
inline constexpr uint16_t DOUBLE_PAWN_PUSH= 1; // 0001
inline constexpr uint16_t KING_CASTLE= 2;      // 0010
inline constexpr uint16_t QUEEN_CASTLE= 3;     // 0011
inline constexpr uint16_t CAPTURE= 4;          // 0100
inline constexpr uint16_t EN_PASSANT= 5;       // 0101

// 6 AND 7 are reserved for future use (e.g., special move types or additional flags)

// Promotions (We don't need to specify color, just the piece type)
inline constexpr uint16_t PROMO_KNIGHT= 8; // 1000
inline constexpr uint16_t PROMO_BISHOP= 9; // 1001
inline constexpr uint16_t PROMO_ROOK= 10;  // 1010
inline constexpr uint16_t PROMO_QUEEN= 11; // 1011

// Capture Promotions
inline constexpr uint16_t CAPTURE_PROMO_KNIGHT= 12; // 1100
inline constexpr uint16_t CAPTURE_PROMO_BISHOP= 13; // 1101
inline constexpr uint16_t CAPTURE_PROMO_ROOK= 14;   // 1110
inline constexpr uint16_t CAPTURE_PROMO_QUEEN= 15;  // 1111
} // namespace MoveFlag

inline constexpr Move Encode(int from, int to, uint16_t flag= MoveFlag::QUIET) {
    return from | (to << 6) | (flag << 12);
}

inline constexpr int GetFrom(Move move) {
    return move & 0b111111; // Extract bits 0-5
}
inline constexpr int GetTo(Move move) {
    return (move >> 6) & 0b111111; // Extract bits 6-11
}
inline constexpr uint16_t GetFlag(Move move) {
    return (move >> 12) & 0b1111; // Extract bits 12-15 (4 bits)
}

inline std::string toString(Move move) {
    int from= GetFrom(move);
    int to= GetTo(move);
    uint16_t flag= GetFlag(move);

    std::string s= Representation::Coordinates::ToAlgebraic(from) + Representation::Coordinates::ToAlgebraic(to);

    if(flag & PROMOTION_MASK) {
        constexpr char promoChars[4]= {
            'n', // PROMO_KNIGHT
            'b', // PROMO_BISHOP
            'r', // PROMO_ROOK
            'q'  // PROMO_QUEEN
        };
        auto pieceIndex= flag & 0b0011; // Get the last 2 bits to determine promotion type (0-3)
        s+= promoChars[pieceIndex];
    }
    return s;
}
} // namespace Knilb::Representation::Moves