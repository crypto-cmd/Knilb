#pragma once

#include "Bitboard.hpp"
#include "Coordinate.hpp"
#include "Move.hpp"
#include "Piece.hpp"
#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace Knilb::Representation {

// Struct to save the state of the game before a move is made
struct GameState {
    Pieces::Piece capturedPiece;
    uint8_t castlingRights;
    int enPassantIndex;
    int halfMoveClock;
    uint64_t zobristHash; // Saved hash

    Moves::Move move; // The move that was made to reach this state

    Coordinates::Coordinate whiteKingPos; // Saved white king position
    Coordinates::Coordinate blackKingPos; // Saved black king position

    // incremental evaluation state
    int openingScore;
    int endgameScore;
    int phaseCounter;
};

class Board {
  private:
    // The board representation
    Bitboards::Bitboard bitboards[12];
    Bitboards::Bitboard occupancies[2]; // 0: White, 1: Black

    int openingScore= 0; // Opening material+PST
    int endgameScore= 0; // Endgame material+PST
    // running game phase counter (0..MAX_PHASE)
    // keep a private copy of the constants to avoid pulling in Evaluation.hpp
    static constexpr int PHASE_QUEEN= 4;
    static constexpr int PHASE_ROOK= 2;
    static constexpr int PHASE_BISHOP= 1;
    static constexpr int PHASE_KNIGHT= 1;
    static constexpr int MAX_PHASE= 2 * PHASE_QUEEN + 4 * PHASE_ROOK + 4 * PHASE_BISHOP + 4 * PHASE_KNIGHT;

    int phaseCounter= MAX_PHASE;

  public:
    // helper to access normalized phase [0.0 .. 1.0]
    // 1 means opening, 0 means endgame. Useful for tuning and scaling evaluation terms.
    float gamePhase() const {
        return static_cast<float>(phaseCounter) / MAX_PHASE;
    }
    // accessors for evaluation
    int getOpeningScore() const { return openingScore; }
    int getEndgameScore() const { return endgameScore; }
    int getPhaseCounter() const { return phaseCounter; }

  public:
    Bitboards::Bitboard GetBitBoard(Pieces::Piece piece) const {
        if(piece == Pieces::Kind::NONE) return 0ULL;

        auto pieceIndex= [&](Pieces::Piece p) -> int {
            auto k= Pieces::GetKind(p);
            int idx= -1;
            switch(k) {
            case Pieces::Kind::PAWN: idx= 0; break;
            case Pieces::Kind::KNIGHT: idx= 1; break;
            case Pieces::Kind::BISHOP: idx= 2; break;
            case Pieces::Kind::ROOK: idx= 3; break;
            case Pieces::Kind::QUEEN: idx= 4; break;
            case Pieces::Kind::KING: idx= 5; break;
            default: return -1;
            }
            if(Pieces::IsColor(p, Pieces::Color::BLACK)) idx+= 6;
            return idx;
        };

        int kind= Pieces::GetKind(piece);
        int color= Pieces::GetColor(piece);
        // In the form of <Color> so combine all pieces of that color (occupancy)
        if(kind == Pieces::Kind::NONE) {
            if(color == Pieces::Color::WHITE) return occupancies[0];
            if(color == Pieces::Color::BLACK) return occupancies[1];
            throw std::invalid_argument("Something went horribly wrong");
        }
        // In the form of <Kind> so get both colors of that piece and combine
        if(color == Pieces::Color::NO_COLOR) {
            auto idx= pieceIndex(piece);
            if(idx == -1) throw std::invalid_argument("Invalid piece");
            return bitboards[idx] | bitboards[6 + idx];
        }

        // In the form of <Color|Kind> so get the specific piece

        return bitboards[pieceIndex(piece)];
    }

    // Helper: Returns true if move is a pawn move or capture
    static bool isIrreversibleMove(const Moves::Move& move, Pieces::Piece capturedPiece, const Board* board) {
        using namespace Moves;
        if(capturedPiece != Pieces::Kind::NONE) return true;
        int from= GetFrom(move);
        if(from < 0 || from >= 64 || !board) return false;
        return Pieces::IsKind(board->squares[from], Pieces::Kind::PAWN);
    }

    bool isTwofoldRepetition() const {
        // Keep going back through the history until we find a position that is not the same as the current one or we run out of history or we find an irreversible move (pawn move or capture)
        for(int i= historyIndex - 1; i >= 0; i--) {
            if(game_history[i].zobristHash == zobristHash) {
                return true;
            }
            // If we encounter an irreversible move, we can stop checking further back
            if(isIrreversibleMove(game_history[i].move, game_history[i].capturedPiece, this)) break;
        }
        return false;
    }

  public:
    // Standard starting position FEN
    static constexpr const char* STARTING_POS= "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Pieces::Piece squares[64];

    // History stack for undoing moves
    GameState game_history[2048]; // Assuming a maximum of 256 moves in a game
    int historyIndex= 0;

    // Game State Variables
    Pieces::Color activeColor= Pieces::Color::WHITE;

    Coordinates::Coordinate whiteKingPos= Coordinates::E1; // Default starting position
    Coordinates::Coordinate blackKingPos= Coordinates::E8; // Default starting position
    // Castling Bitmasks
    static constexpr uint8_t CASTLE_WK= 1; // White King-side
    static constexpr uint8_t CASTLE_WQ= 2; // White Queen-side
    static constexpr uint8_t CASTLE_BK= 4; // Black King-side
    static constexpr uint8_t CASTLE_BQ= 8; // Black Queen-side

    uint8_t castlingRights= 0;
    int enPassantIndex= -1; // -1 if no en passant available
    int halfMoveClock= 0;   // 50-move rule counter
    int fullMoveNumber= 1;

    // Zobrist Hash for Transposition Table
    uint64_t zobristHash= 0;

    // Constructors and Core Methods
    Board();
    void setFen(const std::string& fen);
    void print() const;

    // Move Execution
    void makeMove(const Moves::Move& move);
    void undoMove();

    // Calculates the hash from scratch (slow, used for verification/initialization)
    uint64_t calculateHash() const;

    void updateOccupancies() {
        occupancies[0]= 0;
        occupancies[1]= 0;
        // Combine white pieces
        for(int i= 0; i < 6; ++i) occupancies[0]|= bitboards[i];
        // Combine black pieces
        for(int i= 6; i < 12; ++i) occupancies[1]|= bitboards[i];
    }

    bool isInCheck();
};

} // namespace Knilb::Representation