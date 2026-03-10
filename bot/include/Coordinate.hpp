#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

namespace Knilb::Representation::Coordinates {
using Coordinate= int; // [rank: 3 bits][file: 3 bits] - 6 bits total, values 0-63

inline constexpr int GetRank(Coordinate sq) { return sq >> 3; }
inline constexpr int GetFile(Coordinate sq) { return sq & 7; }
inline constexpr Coordinate Create(int file, int rank) { return (rank << 3) | file; }

inline constexpr bool IsValid(Coordinate sq) { return sq < 64 && sq >= 0; }

// Example: If you ever need to flip the board from White's perspective to Black's
inline constexpr int Flip(Coordinate sq) { return sq ^ 56; }

inline constexpr bool IsInRank(Coordinate sq, int rank) { return GetRank(sq) == rank; }
inline constexpr bool IsInFile(Coordinate sq, int file) { return GetFile(sq) == file; }

// Predefined constants for easy reference
// clang-format off
enum {
    A_FILE= 0, B_FILE= 1, C_FILE= 2, D_FILE= 3, E_FILE= 4, F_FILE= 5, G_FILE= 6, H_FILE= 7
};
enum : Coordinate {
    A1 = 0, B1 = 1, C1 = 2, D1 = 3, E1 = 4, F1 = 5, G1 = 6, H1 = 7,
    A2 = 8, B2 = 9, C2 = 10, D2 = 11, E2 = 12, F2 = 13, G2 = 14, H2 = 15,
    A3 = 16, B3 = 17, C3 = 18, D3 = 19, E3 = 20, F3 = 21, G3 = 22, H3 = 23,
    A4 = 24, B4 = 25, C4 = 26, D4 = 27, E4 = 28, F4 = 29, G4 = 30, H4 = 31,
    A5 = 32, B5 = 33, C5 = 34, D5 = 35, E5 = 36, F5 = 37, G5 = 38, H5 = 39,
    A6 = 40, B6 = 41, C6 = 42, D6 = 43, E6 = 44, F6 = 45, G6 = 46, H6 = 47,
    A7 = 48, B7 = 49, C7 = 50, D7 = 51, E7 = 52, F7 = 53, G7 = 54, H7 = 55,
    A8 = 56, B8 = 57, C8 = 58, D8 = 59, E8 = 60, F8 = 61, G8 = 62, H8 = 63
};
// clang-format on

// Convert coordinate to algebraic notation (e.g., 0 -> "a1", 63 -> "h8")
inline std::string ToAlgebraic(Coordinate sq) {
    if(!IsValid(sq)) throw std::out_of_range("Invalid coordinate");
    char file= 'a' + GetFile(sq);
    char rank= '1' + GetRank(sq);
    return std::string{file, rank};
}

// Convert algebraic notation to coordinate (e.g., "a1" -> 0, "h8" -> 63)
inline constexpr Coordinate FromAlgebraic(const std::string_view alg) {
    if(alg.length() != 2) throw std::invalid_argument("Invalid algebraic notation");
    char fileChar= alg[0];
    char rankChar= alg[1];
    if(fileChar < 'a' || fileChar > 'h' || rankChar < '1' || rankChar > '8') {
        throw std::invalid_argument("Invalid algebraic notation");
    }
    int file= fileChar - 'a';
    int rank= rankChar - '1';
    return Create(file, rank);
}
}; // namespace Knilb::Representation::Coordinates
