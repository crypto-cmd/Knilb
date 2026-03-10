
#pragma once
#include "Board.hpp"
#include "Piece.hpp"
namespace Knilb::Engine::Evaluation {
using namespace Knilb::Representation;
namespace _Internal {
inline constexpr int PieceValues[7]= {0, 100, 300, 350, 500, 900, 20000}; // NONE, PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING
}
inline constexpr int INF= 1000000;
inline constexpr int MATE_VAL= 900000;
inline constexpr int STALEMATE_SCORE= 0;
inline constexpr int MATE(int ply) { return MATE_VAL - ply; } // Closer mates are better
inline constexpr int GetPieceValue(Pieces::Piece piece) {
    if(piece == Pieces::Kind::NONE) return 0;
    int type= Pieces::GetKind(piece);
    return _Internal::PieceValues[type];
}
int evaluate(const Representation::Board& board);
int SEE(const Representation::Board& board, Representation::Moves::Move move);
int EvaluateKingSafety(const Representation::Board& board, Pieces::Color color);
// phase helpers moved into header so Board.cpp can share them
inline constexpr int PHASE_QUEEN= 4;
inline constexpr int PHASE_ROOK= 2;
inline constexpr int PHASE_BISHOP= 1;
inline constexpr int PHASE_KNIGHT= 1;
inline constexpr int MAX_PHASE= 2 * PHASE_QUEEN + 4 * PHASE_ROOK + 4 * PHASE_BISHOP + 4 * PHASE_KNIGHT;

inline int GetPhaseWeight(Pieces::Piece piece) {
    switch(Pieces::GetKind(piece)) {
    case Pieces::Kind::QUEEN: return PHASE_QUEEN;
    case Pieces::Kind::ROOK: return PHASE_ROOK;
    case Pieces::Kind::BISHOP: return PHASE_BISHOP;
    case Pieces::Kind::KNIGHT: return PHASE_KNIGHT;
    default: return 0;
    }
}

namespace Tuning {
// Tunable parameters for evaluation function
inline int PassedPawnBonus= 50;
inline int HostilityBonus= 10;
inline int EndgameKingBonus= 20;
inline int OpponentPiecePenalty= 10;
}; // namespace Tuning
} // namespace Knilb::Engine::Evaluation
