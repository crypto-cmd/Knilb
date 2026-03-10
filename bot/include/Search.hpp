#pragma once
#include "Board.hpp"
#include "Evaluation.hpp"
#include "Move.hpp"
#include "MoveGeneration.hpp"
#include "TranspositionTable.hpp"
#include <algorithm>
namespace Knilb::Engine::Search {
namespace Killer {
inline Representation::Moves::Move Table[64][2]; // Two killer moves per ply
void Clear() {
    for(int i= 0; i < 64; ++i) {
        Table[i][0]= Representation::Moves::DEFAULT_MOVE;
        Table[i][1]= Representation::Moves::DEFAULT_MOVE;
    }
}
void Store(int ply, Representation::Moves::Move move) {
    if(move == Representation::Moves::DEFAULT_MOVE) return; // Don't store invalid moves
    if(ply >= 64) return;                                   // Ply out of bounds

    Table[ply][1]= Table[ply][0]; // Move the current killer to the second slot
    Table[ply][0]= move;          // Store the new killer move in the first
}

int GetScore(int ply, const Representation::Moves::Move& move) {
    if(ply >= 64) return 0;
    if(move == Table[ply][0]) return 8000; // Primary killer
    if(move == Table[ply][1]) return 4000; // Secondary killer
    return 0;                              // Not a killer move
}
} // namespace Killer

namespace History {
// Define a maximum history score to act as our "gravity" ceiling
constexpr int MAX_HISTORY= 16384;
// History heuristic table: [Color][From][To]
inline int Table[2][64][64];
void Clear() {
    for(int color= 0; color < 2; ++color) {
        for(int from= 0; from < 64; ++from) {
            for(int to= 0; to < 64; ++to) {
                Table[color][from][to]= 0;
            }
        }
    }
}
inline void Store(Representation::Pieces::Color color, int from, int to, int depth) {
    if(color == Representation::Pieces::Color::NO_COLOR || from < 0 || from >= 64 || to < 0 || to >= 64) return;

    // 1. Cap the maximum bonus a single move can receive
    int bonus= depth * depth;
    if(bonus > 400) bonus= 400;

    // 2. Apply decay/gravity formula
    int& score= Table[color == Representation::Pieces::Color::WHITE ? 0 : 1][from][to];
    score+= bonus - score * abs(bonus) / MAX_HISTORY;
}

inline int GetScore(Representation::Pieces::Color color, int from, int to) {
    if(color == Representation::Pieces::Color::NO_COLOR || from < 0 || from >= 64 || to < 0 || to >= 64) return 0;
    return Table[color == Representation::Pieces::Color::WHITE ? 0 : 1][from][to];
}
} // namespace History
namespace MoveOrdering {
inline int ScoreMove(const Representation::Moves::Move& move, const Representation::Moves::Move& ttMove, const Representation::Board& board, int ply) {
    using namespace Representation;
    using namespace Engine::Evaluation;
    int score= 0;
    if(move == Moves::DEFAULT_MOVE) return -1; // Invalid move, lowest priority
    if(ttMove == move) return 50000;           // TT move highest priority

    // MVV-LVA for captures
    auto flag= Moves::GetFlag(move);
    if(flag & Representation::Moves::CAPTURE_MASK) {
        int seeValue= Evaluation::SEE(board, move); // Returns e.g. +100 for winning a pawn, -800 for losing a queen

        if(seeValue >= 0) {
            // Good or equal captures: score them way above killer moves
            score+= 20000 + seeValue;
        } else {
            // Bad captures (e.g., QxP protected by P): score them below killer moves
            score+= 1000 + seeValue;
        }
    }
    score+= Killer::GetScore(ply, move);
    score+= History::GetScore(board.activeColor, Representation::Moves::GetFrom(move), Representation::Moves::GetTo(move));
    return score;
}
inline void SortMoves(const Representation::Board& board, MoveGeneration::MoveList& moveList, Engine::Search::TranspositionTable& tt, int ply) {
    auto& moves= moveList.moves;
    int count= moveList.count;
    auto ttMove= tt.probe(board.zobristHash).zobristHash == board.zobristHash ? tt.probe(board.zobristHash).bestMove : Representation::Moves::DEFAULT_MOVE;
    std::vector<std::pair<Representation::Moves::Move, int>> scoredMoves;
    scoredMoves.reserve(count);
    for(int i= 0; i < count; ++i) {
        auto move= moves[i];
        auto flag= Representation::Moves::GetFlag(move);
        int score= ScoreMove(move, ttMove, board, ply);

        scoredMoves.emplace_back(move, score);
    }
    std::sort(scoredMoves.begin(), scoredMoves.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    for(int i= 0; i < count; ++i) {
        moves[i]= scoredMoves[i].first;
    }
}

inline void OrderTTMove(MoveGeneration::MoveList& moveList, const Representation::Moves::Move& ttMove) {
    if(ttMove == Representation::Moves::DEFAULT_MOVE) return;

    for(int i= 0; i < moveList.count; ++i) {
        if(moveList.moves[i] == ttMove) {
            // Swap the TT move to index 0
            std::swap(moveList.moves[0], moveList.moves[i]);
            break; // Stop looking, there's only one TT move
        }
    }
};

}; // namespace MoveOrdering

namespace LMR {
// Precompute natural logarithm values for 0 to 63 to avoid runtime log calculations
constexpr double LN[64]= {
    0.000000, 0.000000, 0.693147, 1.098612, 1.386294, 1.609438, 1.791759, 1.945910,
    2.079442, 2.197225, 2.302585, 2.397895, 2.484907, 2.564949, 2.639057, 2.708050,
    2.772589, 2.833213, 2.890372, 2.944439, 2.995732, 3.044522, 3.091042, 3.135494,
    3.178054, 3.218876, 3.258097, 3.295837, 3.332205, 3.367296, 3.401197, 3.433987,
    3.465736, 3.496508, 3.526361, 3.555348, 3.583519, 3.610918, 3.637586, 3.663562,
    3.688879, 3.713572, 3.737670, 3.761200, 3.784190, 3.806662, 3.828641, 3.850148,
    3.871201, 3.891820, 3.912023, 3.931826, 3.951244, 3.970292, 3.988984, 4.007333,
    4.025352, 4.043051, 4.060443, 4.077537, 4.094345, 4.110874, 4.127134, 4.143135};

constexpr auto computeTable() {
    std::array<std::array<int, 64>, 64> table{};
    for(int depth= 0; depth < 64; ++depth) {
        for(int moveIndex= 0; moveIndex < 64; ++moveIndex) {
            if(depth > 0 && moveIndex > 0) {
                double reduction= 1.25 + (LN[depth] * LN[moveIndex]) / 2.5;
                table[depth][moveIndex]= static_cast<int>(reduction);
            } else {
                table[depth][moveIndex]= 0; // No reduction for first few moves and shallow depths
                continue;
            }
        }
    }
    return table;
}

std::array<std::array<int, 64>, 64> Table= computeTable();
} // namespace LMR

}; // namespace Knilb::Engine::Search