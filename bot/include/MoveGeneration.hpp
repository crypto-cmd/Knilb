#pragma once

#include "Board.hpp"
#include "Coordinate.hpp"
#include "Move.hpp"
#include "Precomputed.hpp"
#include <array>
#include <span>
#include <vector>

namespace Knilb::MoveGeneration {
using namespace Representation;
using namespace Representation::Pieces;
using namespace Representation::Moves;
using namespace Representation::Coordinates;

constexpr int MAX_MOVES= 256;
struct MoveList {
    Move moves[MAX_MOVES];
    int count= 0;

    void push_back(Move m) {
        if(count >= MAX_MOVES) {
            throw std::overflow_error("MoveList capacity exceeded");
        }
        moves[count++]= m;
    }
    void clear() { count= 0; }
    bool empty() const { return count == 0; }
    int size() const { return count; }

    // Iterators so range-based for loops still work
    Move* begin() { return &moves[0]; }
    Move* end() { return &moves[count]; }
    Move& operator[](int i) { return moves[i]; }
};

namespace Generator {

void GenerateMoves(const Board& board, MoveList& moveList);
void GeneratePawnMoves(const Board& board, MoveList& moves);
void GenerateHoppingMoves(const Board& board, MoveList& moves, Bitboards::Bitboard piecesOnBitboard, const std::array<Bitboards::Bitboard, 64>& AttackTable);
void GenerateSlidingMoves(const Board& board, MoveList& moves);
void GenerateCastlingMoves(const Board& board, MoveList& moves);

void GenerateCaptureMoves(const Board& board, MoveList& moveList);
void GenerateCaptureMoves(const Knilb::Representation::Board& board, MoveList& moveList);
void GeneratePawnCapturesAndPromotions(const Knilb::Representation::Board& board, MoveList& moves);
void GenerateHoppingCaptures(const Knilb::Representation::Board& board, MoveList& moves, Bitboards::Bitboard piecesOnBitboard, const std::array<Bitboards::Bitboard, 64>& AttackTable);
void GenerateSlidingCaptures(const Knilb::Representation::Board& board, MoveList& moves);

} // namespace Generator
bool IsSquareAttacked(const Board& board, Coordinate square, Pieces::Color attackerColor);
bool IsLegalPosition(const Board& board);

constexpr std::array<Bitboards::Bitboard, 64> KnightAttacks= []() {
    std::array<Bitboards::Bitboard, 64> attacks{};
    int row_offsets[]= {-2, -1, 1, 2, 2, 1, -1, -2};
    int col_offsets[]= {1, 2, 2, 1, -1, -2, -2, -1};

    for(int sq= 0; sq < 64; sq++) {
        int r= sq / 8;
        int c= sq % 8;

        for(int i= 0; i < 8; i++) {
            int targetRow= r + row_offsets[i];
            int targetCol= c + col_offsets[i];

            if(targetRow >= 0 && targetRow < 8 && targetCol >= 0 && targetCol < 8) {
                int targetSq= targetRow * 8 + targetCol;
                Bitboards::SetBit(attacks[sq], targetSq);
            }
        }
    }
    return attacks;
}();

constexpr std::array<Bitboards::Bitboard, 64> KingAttacks= []() {
    std::array<Bitboards::Bitboard, 64> attacks{};
    // Neighbor offsets (row, col)
    int row_offsets[]= {-1, -1, -1, 0, 0, 1, 1, 1};
    int col_offsets[]= {-1, 0, 1, -1, 1, -1, 0, 1};

    for(int sq= 0; sq < 64; sq++) {
        int r= sq / 8;
        int c= sq % 8;

        for(int i= 0; i < 8; i++) {
            int targetRow= r + row_offsets[i];
            int targetCol= c + col_offsets[i];

            // Boundary check
            if(targetRow >= 0 && targetRow < 8 && targetCol >= 0 && targetCol < 8) {
                int targetSq= targetRow * 8 + targetCol;
                Bitboards::SetBit(attacks[sq], targetSq);
            }
        }
    }
    return attacks;
}();
constexpr std::array<std::array<Bitboards::Bitboard, 64>, 8> Rays= []() {
    std::array<std::array<Bitboards::Bitboard, 64>, 8> Attacks{};
    // Loop over every square
    for(int sq= 0; sq < 64; ++sq) {
        // Loop over all 8 directions (N, S, E, W, NW, SE, NE, SW)
        for(int dir= 0; dir < 8; ++dir) {
            Attacks[dir][sq]= 0ULL; // Start empty

            // 1. How far can we go? (Already computed!)
            int maxSteps= Precomputed::Tables.NumSquaresToEdge[sq][dir];

            // 2. What is the step size?
            int offset= Precomputed::DirectionOffsets[dir];

            int currentSq= sq;

            // 3. Walk the ray
            for(int step= 0; step < maxSteps; ++step) {
                currentSq+= offset; // Move one step
                Bitboards::SetBit(Attacks[dir][sq], currentSq);
            }
        }
    }
    return Attacks;
}();

constexpr std::array<std::array<Bitboards::Bitboard, 64>, 2> PawnAttacks= []() {
    std::array<std::array<Bitboards::Bitboard, 64>, 2> Attacks{};
    // White Pawns (Color 0) capture NorthWest (+7) and NorthEast (+9)
    // Black Pawns (Color 1) capture SouthEast (-7) and SouthWest (-9)

    for(int sq= 0; sq < 64; sq++) {
        int r= sq / 8;
        int c= sq % 8;

        // --- WHITE ATTACKS (Index 0) ---
        Attacks[0][sq]= 0ULL;
        if(r < 7) {                                              // Can't attack from Rank 8
            if(c > 0) Bitboards::SetBit(Attacks[0][sq], sq + 7); // NW
            if(c < 7) Bitboards::SetBit(Attacks[0][sq], sq + 9); // NE
        }

        // --- BLACK ATTACKS (Index 1) ---
        Attacks[1][sq]= 0ULL;
        if(r > 0) {                                              // Can't attack from Rank 1
            if(c < 7) Bitboards::SetBit(Attacks[1][sq], sq - 7); // SE
            if(c > 0) Bitboards::SetBit(Attacks[1][sq], sq - 9); // SW
        }
    }
    return Attacks;
}();

// Private
// Helper: Get attacks for a specific direction
inline auto GetRayAttacks= [](int sq, int dir, Bitboards::Bitboard all_pieces) {
    // 1. Get the full ray from the lookup table
    auto attack= Rays[dir][sq];

    // 2. Intersect with occupancy to find blockers
    Bitboards::Bitboard blockers= attack & all_pieces;

    // 3. If there are blockers, cut the ray short
    if(blockers) {
        // Find the first square we hit
        // Positive directions (N, E, NE, NW) use LSB (Scan Forward)
        // Negative directions (S, W, SE, SW) use MSB (Scan Backward)
        // Indices: 0=N, 2=E, 4=NE, 5=NW are "Positive" relative to bit indices?
        // Actually:
        // North(+8), East(+1), NE(+9), NW(+7) -> increasing indices -> LSB
        // South(-8), West(-1), SE(-7), SW(-9) -> decreasing indices -> MSB
        // Precomputed order: 0=N, 1=S, 2=E, 3=W, 4=NW, 5=SE, 6=NE, 7=SW

        int blocker_sq;
        if(dir == 0 || dir == 2 || dir == 4 || dir == 6)
            blocker_sq= Bitboards::GetLSBIndex(blockers);
        else
            blocker_sq= Bitboards::GetMSBIndex(blockers);

        // XOR trick: The ray from the blocker continues to the edge.
        // XORing it removes the "shadow" behind the blocker.
        // We OR the blocker_sq back in because we can capture it.
        attack= (attack ^ Rays[dir][blocker_sq]) | (1ULL << blocker_sq);
    }
    return attack;
};
inline auto GetRookAttacks= [](int sq, Bitboards::Bitboard all_pieces) {
    return GetRayAttacks(sq, 0, all_pieces) | // North
           GetRayAttacks(sq, 1, all_pieces) | // South
           GetRayAttacks(sq, 2, all_pieces) | // East
           GetRayAttacks(sq, 3, all_pieces);  // West
};

inline auto GetBishopAttacks= [](int sq, Bitboards::Bitboard all_pieces) {
    return GetRayAttacks(sq, 4, all_pieces) | // NE
           GetRayAttacks(sq, 5, all_pieces) | // NW
           GetRayAttacks(sq, 6, all_pieces) | // SE
           GetRayAttacks(sq, 7, all_pieces);  // SW
};
}; // namespace Knilb::MoveGeneration