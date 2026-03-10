#include "MoveGeneration.hpp"
#include "Board.hpp"
#include "Coordinate.hpp"
#include "Move.hpp"
#include "Piece.hpp"
#include <algorithm>
#include <span>
#include <vector>

using namespace Knilb::Representation;
using namespace Knilb::Representation::Bitboards;
// --- Helper: Attack Detection ---

// Returns true if 'square' is being attacked by 'attackerColor'
namespace Knilb::MoveGeneration {
bool IsSquareAttacked(const Board& board, Coordinate square, Pieces::Color attackerColor) {
    // Pawns
    auto defenderColor= attackerColor == Pieces::Color::WHITE ? Pieces::Color::BLACK : Pieces::Color::WHITE;
    auto defender_ci= defenderColor == Pieces::Color::WHITE ? 0 : 1;
    auto pawnBB= board.GetBitBoard(attackerColor | Pieces::Kind::PAWN);
    if(pawnBB & PawnAttacks[defender_ci][square]) return true;

    // Knights
    auto knightBB= board.GetBitBoard(attackerColor | Pieces::Kind::KNIGHT);
    if(knightBB & KnightAttacks[square]) return true;

    // King
    auto kingBB= board.GetBitBoard(attackerColor | Pieces::Kind::KING);
    if(kingBB & KingAttacks[square]) return true;

    // Sliding pieces (Rooks, Bishops, Queens)
    auto allPieces= board.GetBitBoard(Pieces::Color::WHITE) | board.GetBitBoard(Pieces::Color::BLACK);

    auto rookBB= board.GetBitBoard(attackerColor | Pieces::Kind::ROOK);
    auto bishopBB= board.GetBitBoard(attackerColor | Pieces::Kind::BISHOP);
    auto queenBB= board.GetBitBoard(attackerColor | Pieces::Kind::QUEEN);

    auto rookAttacks= GetRookAttacks(square, allPieces);
    if((rookBB | queenBB) & rookAttacks) return true;

    auto bishopAttacks= GetBishopAttacks(square, allPieces);
    if((bishopBB | queenBB) & bishopAttacks) return true;

    return false;
}

bool IsLegalPosition(const Board& board) {
    Pieces::Color colorThatShouldNotBeInCheck= board.activeColor == Pieces::Color::WHITE ? Pieces::Color::BLACK : Pieces::Color::WHITE;
    auto kingBB= board.GetBitBoard(colorThatShouldNotBeInCheck | Pieces::Kind::KING);

    auto kingSq= Bitboards::GetLSBIndex(kingBB);
    Pieces::Color attackerColor= board.activeColor;

    return !IsSquareAttacked(board, kingSq, attackerColor);
}
void Generator::GenerateMoves(const Knilb::Representation::Board& board, Knilb::MoveGeneration::MoveList& moveList) {

    auto knights= board.GetBitBoard(board.activeColor | Pieces::Kind::KNIGHT);
    auto king= board.GetBitBoard(board.activeColor | Pieces::Kind::KING);
    GeneratePawnMoves(board, moveList);
    GenerateHoppingMoves(board, moveList, knights, KnightAttacks);
    GenerateHoppingMoves(board, moveList, king, KingAttacks);
    GenerateSlidingMoves(board, moveList);
    GenerateCastlingMoves(board, moveList);
}

void Generator::GeneratePawnMoves(const Board& board, MoveList& moves) {
    auto pawns= board.GetBitBoard(board.activeColor | Pieces::Kind::PAWN);
    auto allOccupancy= board.GetBitBoard(Pieces::Color::WHITE) | board.GetBitBoard(Pieces::Color::BLACK);
    auto emptySquares= ~allOccupancy;
    auto opponentOccupancy= board.GetBitBoard(board.activeColor == Pieces::Color::WHITE ? Pieces::Color::BLACK : Pieces::Color::WHITE);

    if(board.activeColor == Pieces::Color::WHITE) {
        // Single push
        auto singlePush= (pawns << 8) & emptySquares;
        auto doublePush= ((singlePush & Bitboards::Rank3) << 8) & emptySquares; // Who survives on rank 3 after single push can double push

        auto captureRight= ((pawns & ~Bitboards::FileH) << 9) & opponentOccupancy; // Capture to the right (NE)
        auto captureLeft= ((pawns & ~Bitboards::FileA) << 7) & opponentOccupancy;  // Capture to the left (NW)

        auto c1= captureLeft;
        while(c1) {
            int to= Bitboards::PopLSB(c1);
            if(to >= 56) {

                moves.push_back(Moves::Encode(to - 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_QUEEN));
                moves.push_back(Moves::Encode(to - 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_ROOK));
                moves.push_back(Moves::Encode(to - 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_BISHOP));
                moves.push_back(Moves::Encode(to - 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_KNIGHT));
            } else moves.push_back(Moves::Encode(to - 7, to, Moves::MoveFlag::CAPTURE));
        }
        auto c2= captureRight;
        while(c2) {
            int to= Bitboards::PopLSB(c2);
            if(to >= 56) {
                moves.push_back(Moves::Encode(to - 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_QUEEN));
                moves.push_back(Moves::Encode(to - 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_ROOK));
                moves.push_back(Moves::Encode(to - 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_BISHOP));
                moves.push_back(Moves::Encode(to - 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_KNIGHT));
            } else moves.push_back(Moves::Encode(to - 9, to, Moves::MoveFlag::CAPTURE));
        }

        auto s= singlePush;
        while(s) {
            int to= Bitboards::PopLSB(s);
            if(to >= 56) {
                moves.push_back(Moves::Encode(to - 8, to, Moves::MoveFlag::PROMO_QUEEN));
                moves.push_back(Moves::Encode(to - 8, to, Moves::MoveFlag::PROMO_ROOK));
                moves.push_back(Moves::Encode(to - 8, to, Moves::MoveFlag::PROMO_BISHOP));
                moves.push_back(Moves::Encode(to - 8, to, Moves::MoveFlag::PROMO_KNIGHT));
            } else moves.push_back(Moves::Encode(to - 8, to));
        }
        s= doublePush;
        while(s) {
            int to= Bitboards::PopLSB(s);
            moves.push_back(Moves::Encode(to - 16, to, Moves::MoveFlag::DOUBLE_PAWN_PUSH));
        }

        // En Passant
        if(board.enPassantIndex != -1) {
            int epSquare= board.enPassantIndex;
            if(((pawns & ~Bitboards::FileH) << 9) & (1ULL << epSquare)) {
                moves.push_back(Moves::Encode(epSquare - 9, epSquare, Moves::MoveFlag::EN_PASSANT));
            }
            if(((pawns & ~Bitboards::FileA) << 7) & (1ULL << epSquare)) {
                moves.push_back(Moves::Encode(epSquare - 7, epSquare, Moves::MoveFlag::EN_PASSANT));
            }
        }

    } else {
        // Single push
        auto singlePush= (pawns >> 8) & emptySquares;
        auto doublePush= ((singlePush & Bitboards::Rank6) >> 8) & emptySquares; // Who survives on rank 6 after single push can double push

        auto captureRight= ((pawns & ~Bitboards::FileH) >> 7) & opponentOccupancy; // Capture to the right (SE)
        auto captureLeft= ((pawns & ~Bitboards::FileA) >> 9) & opponentOccupancy;  // Capture to the left (SW)

        auto c1= captureLeft;
        while(c1) {
            int to= Bitboards::PopLSB(c1);
            if(to <= 7) {

                moves.push_back(Moves::Encode(to + 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_QUEEN));
                moves.push_back(Moves::Encode(to + 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_ROOK));
                moves.push_back(Moves::Encode(to + 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_BISHOP));
                moves.push_back(Moves::Encode(to + 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_KNIGHT));
            } else moves.push_back(Moves::Encode(to + 9, to, Moves::MoveFlag::CAPTURE));
        }
        auto c2= captureRight;
        while(c2) {
            int to= Bitboards::PopLSB(c2);
            if(to <= 7) {
                moves.push_back(Moves::Encode(to + 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_QUEEN));
                moves.push_back(Moves::Encode(to + 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_ROOK));
                moves.push_back(Moves::Encode(to + 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_BISHOP));
                moves.push_back(Moves::Encode(to + 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_KNIGHT));
            } else moves.push_back(Moves::Encode(to + 7, to, Moves::MoveFlag::CAPTURE));
        }

        auto s= singlePush;
        while(s) {
            int to= Bitboards::PopLSB(s);
            if(to <= 7) {
                moves.push_back(Moves::Encode(to + 8, to, Moves::MoveFlag::PROMO_QUEEN));
                moves.push_back(Moves::Encode(to + 8, to, Moves::MoveFlag::PROMO_ROOK));
                moves.push_back(Moves::Encode(to + 8, to, Moves::MoveFlag::PROMO_BISHOP));
                moves.push_back(Moves::Encode(to + 8, to, Moves::MoveFlag::PROMO_KNIGHT));
            } else moves.push_back(Moves::Encode(to + 8, to));
        }
        s= doublePush;
        while(s) {
            int to= Bitboards::PopLSB(s);
            moves.push_back(Moves::Encode(to + 16, to, Moves::MoveFlag::DOUBLE_PAWN_PUSH));
        }

        // En Passant
        if(board.enPassantIndex != -1) {
            int epSquare= board.enPassantIndex;
            if(((pawns & ~Bitboards::FileH) >> 7) & (1ULL << epSquare)) {
                moves.push_back(Moves::Encode(epSquare + 7, epSquare, Moves::MoveFlag::EN_PASSANT));
            }
            if(((pawns & ~Bitboards::FileA) >> 9) & (1ULL << epSquare)) {
                moves.push_back(Moves::Encode(epSquare + 9, epSquare, Moves::MoveFlag::EN_PASSANT));
            }
        }
    }
}

void Generator::GenerateHoppingMoves(const Board& board, MoveList& moves, Bitboards::Bitboard piecesOnBitboard, const std::array<Bitboards::Bitboard, 64>& AttackTable) {

    // 2. We can move anywhere EXCEPT squares occupied by our own pieces
    auto my_pieces= board.GetBitBoard(board.activeColor);
    auto opp_pieces= board.GetBitBoard(board.activeColor == Pieces::WHITE ? Pieces::Color::BLACK : Pieces::Color::WHITE);
    auto valid_targets_mask= ~my_pieces;

    // 3. Loop over each knight
    while(piecesOnBitboard) {
        // Find where the knight is and remove it from the temp bitboard
        int from= Bitboards::PopLSB(piecesOnBitboard);

        // 4. LOOKUP: Get standard attacks for this square
        //    AND mask it with valid targets (removes friendly fire)
        auto attacks= AttackTable[from] & valid_targets_mask;

        // 5. Serialize the moves (turn bits into Move objects)
        while(attacks) {
            int to= Bitboards::PopLSB(attacks);

            bool isCapture= Bitboards::GetBit(opp_pieces, to);

            // Add to list
            moves.push_back(Moves::Encode(from, to, isCapture ? Moves::MoveFlag::CAPTURE : Moves::MoveFlag::QUIET));
        }
    }
}

void Generator::GenerateSlidingMoves(const Board& board, MoveList& moves) {
    // 1. Setup Occupancies
    auto my_pieces= board.GetBitBoard(board.activeColor == Pieces::WHITE ? Pieces::Color::WHITE : Pieces::Color::BLACK);
    auto opp_pieces= board.GetBitBoard(board.activeColor == Pieces::WHITE ? Pieces::Color::BLACK : Pieces::Color::WHITE);
    auto all_pieces= board.GetBitBoard(Pieces::Color::WHITE) | board.GetBitBoard(Pieces::Color::BLACK);

    // 2. Loop Rooks, Bishops, Queens using correct piece encodings
    // (You can merge Queen into Rook+Bishop loops or handle separately)
    using namespace Representation::Pieces;

    Bitboards::Bitboard rookPieces= board.GetBitBoard(board.activeColor | Kind::ROOK);
    Bitboards::Bitboard bishopPieces= board.GetBitBoard(board.activeColor | Kind::BISHOP);
    Bitboards::Bitboard queenPieces= board.GetBitBoard(board.activeColor | Kind::QUEEN);

    // Helper lambda to process a set of pieces
    auto process= [&](Bitboards::Bitboard pieces, bool isDiagonal, bool isOrthogonal) {
        while(pieces) {
            int from= Bitboards::PopLSB(pieces);

            Bitboards::Bitboard attacks= 0;
            if(isDiagonal) attacks|= GetBishopAttacks(from, all_pieces);
            if(isOrthogonal) attacks|= GetRookAttacks(from, all_pieces);

            // Mask out own pieces (cannot capture self)
            attacks&= ~my_pieces;

            // Serialize
            while(attacks) {
                int to= Bitboards::PopLSB(attacks);
                bool isCapture= Bitboards::GetBit(opp_pieces, to);
                moves.push_back(Moves::Encode(from, to, isCapture ? Moves::MoveFlag::CAPTURE : Moves::MoveFlag::QUIET));
            }
        }
    };

    // Run for each type
    process(rookPieces, false, true);   // Rooks
    process(bishopPieces, true, false); // Bishops
    process(queenPieces, true, true);   // Queens
}
void Generator::GenerateCastlingMoves(const Board& board, MoveList& moves) {
    // 1. Get Global Occupancy (All pieces on the board)
    auto allPieces= board.GetBitBoard(Pieces::Color::WHITE) | board.GetBitBoard(Pieces::Color::BLACK);

    // 2. Identify Attacker Color for safety checks
    Color oppColor= (board.activeColor == Pieces::WHITE) ? Pieces::BLACK : Pieces::WHITE;

    if(board.activeColor == Pieces::WHITE) {
        // --- WHITE CASTLING ---

        // Check King Side (e1 -> g1)
        if(board.castlingRights & Board::CASTLE_WK) {
            // A. Path must be empty (f1, g1)
            if((allPieces & Bitboards::WhiteKingSideMask) == 0) {
                // B. Safety Check: e1 (current), f1 (through), g1 (dest) must not be attacked
                if(!IsSquareAttacked(board, Coordinates::E1, oppColor) &&
                   !IsSquareAttacked(board, Coordinates::F1, oppColor) &&
                   !IsSquareAttacked(board, Coordinates::G1, oppColor)) {

                    moves.push_back(Moves::Encode(Coordinates::E1, Coordinates::G1, Moves::MoveFlag::KING_CASTLE));
                }
            }
        }

        // Check Queen Side (e1 -> c1)
        if(board.castlingRights & Board::CASTLE_WQ) {
            // A. Path must be empty (b1, c1, d1)
            if((allPieces & Bitboards::WhiteQueenSideMask) == 0) {
                // B. Safety Check: e1 (current), d1 (through), c1 (dest) must not be attacked
                // Note: b1 does NOT need to be safe, only empty.
                if(!IsSquareAttacked(board, Coordinates::E1, oppColor) &&
                   !IsSquareAttacked(board, Coordinates::D1, oppColor) &&
                   !IsSquareAttacked(board, Coordinates::C1, oppColor)) {

                    moves.push_back(Moves::Encode(Coordinates::E1, Coordinates::C1, Moves::MoveFlag::QUEEN_CASTLE));
                }
            }
        }

    } else {
        // --- BLACK CASTLING ---

        // Check King Side (e8 -> g8)
        if(board.castlingRights & Board::CASTLE_BK) {
            if((allPieces & Bitboards::BlackKingSideMask) == 0) {
                if(!IsSquareAttacked(board, Coordinates::E8, oppColor) &&
                   !IsSquareAttacked(board, Coordinates::F8, oppColor) &&
                   !IsSquareAttacked(board, Coordinates::G8, oppColor)) {

                    moves.push_back(Moves::Encode(Coordinates::E8, Coordinates::G8, Moves::MoveFlag::KING_CASTLE));
                }
            }
        }

        // Check Queen Side (e8 -> c8)
        if(board.castlingRights & Board::CASTLE_BQ) {
            if((allPieces & Bitboards::BlackQueenSideMask) == 0) {
                if(!IsSquareAttacked(board, Coordinates::E8, oppColor) &&
                   !IsSquareAttacked(board, Coordinates::D8, oppColor) &&
                   !IsSquareAttacked(board, Coordinates::C8, oppColor)) {

                    moves.push_back(Moves::Encode(Coordinates::E8, Coordinates::C8, Moves::MoveFlag::QUEEN_CASTLE));
                }
            }
        }
    }
}

void Generator::GenerateCaptureMoves(const Board& board, MoveList& moveList) {
    auto knights= board.GetBitBoard(board.activeColor | Pieces::Kind::KNIGHT);
    auto king= board.GetBitBoard(board.activeColor | Pieces::Kind::KING);

    // We do not call GenerateCastlingMoves because castling is never a capture.
    GeneratePawnCapturesAndPromotions(board, moveList);
    GenerateHoppingCaptures(board, moveList, knights, KnightAttacks);
    GenerateHoppingCaptures(board, moveList, king, KingAttacks);
    GenerateSlidingCaptures(board, moveList);
}
void Generator::GenerateHoppingCaptures(const Board& board, MoveList& moves, Bitboards::Bitboard piecesOnBitboard, const std::array<Bitboards::Bitboard, 64>& AttackTable) {
    auto opp_pieces= board.GetBitBoard(board.activeColor == Pieces::WHITE ? Pieces::Color::BLACK : Pieces::Color::WHITE);

    while(piecesOnBitboard) {
        int from= Bitboards::PopLSB(piecesOnBitboard);

        // Strict capture mask: Only look at squares occupied by the opponent
        auto attacks= AttackTable[from] & opp_pieces;

        while(attacks) {
            int to= Bitboards::PopLSB(attacks);
            // No need for 'isCapture' boolean check here, everything in this mask is a capture
            moves.push_back(Moves::Encode(from, to, Moves::MoveFlag::CAPTURE));
        }
    }
}

void Generator::GenerateSlidingCaptures(const Board& board, MoveList& moves) {
    auto opp_pieces= board.GetBitBoard(board.activeColor == Pieces::WHITE ? Pieces::Color::BLACK : Pieces::Color::WHITE);
    auto all_pieces= board.GetBitBoard(Pieces::Color::WHITE) | board.GetBitBoard(Pieces::Color::BLACK);

    using namespace Representation::Pieces;

    Bitboards::Bitboard rookPieces= board.GetBitBoard(board.activeColor | Kind::ROOK);
    Bitboards::Bitboard bishopPieces= board.GetBitBoard(board.activeColor | Kind::BISHOP);
    Bitboards::Bitboard queenPieces= board.GetBitBoard(board.activeColor | Kind::QUEEN);

    auto process= [&](Bitboards::Bitboard pieces, bool isDiagonal, bool isOrthogonal) {
        while(pieces) {
            int from= Bitboards::PopLSB(pieces);
            Bitboards::Bitboard attacks= 0;

            if(isDiagonal) attacks|= GetBishopAttacks(from, all_pieces);
            if(isOrthogonal) attacks|= GetRookAttacks(from, all_pieces);

            // Strict capture mask
            attacks&= opp_pieces;

            while(attacks) {
                int to= Bitboards::PopLSB(attacks);
                moves.push_back(Moves::Encode(from, to, Moves::MoveFlag::CAPTURE));
            }
        }
    };

    process(rookPieces, false, true);   // Rooks
    process(bishopPieces, true, false); // Bishops
    process(queenPieces, true, true);   // Queens
}

void Generator::GeneratePawnCapturesAndPromotions(const Board& board, MoveList& moves) {
    auto pawns= board.GetBitBoard(board.activeColor | Pieces::Kind::PAWN);
    auto allOccupancy= board.GetBitBoard(Pieces::Color::WHITE) | board.GetBitBoard(Pieces::Color::BLACK);
    auto emptySquares= ~allOccupancy;
    auto opponentOccupancy= board.GetBitBoard(board.activeColor == Pieces::Color::WHITE ? Pieces::Color::BLACK : Pieces::Color::WHITE);

    if(board.activeColor == Pieces::Color::WHITE) {
        auto captureRight= ((pawns & ~Bitboards::FileH) << 9) & opponentOccupancy;
        auto captureLeft= ((pawns & ~Bitboards::FileA) << 7) & opponentOccupancy;

        auto c1= captureLeft;
        while(c1) {
            int to= Bitboards::PopLSB(c1);
            if(to >= 56) {
                moves.push_back(Moves::Encode(to - 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_QUEEN));
                // Usually QS only cares about Queen promos, but you can add underpromotions if desired
            } else moves.push_back(Moves::Encode(to - 7, to, Moves::MoveFlag::CAPTURE));
        }

        auto c2= captureRight;
        while(c2) {
            int to= Bitboards::PopLSB(c2);
            if(to >= 56) {
                moves.push_back(Moves::Encode(to - 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_QUEEN));
            } else moves.push_back(Moves::Encode(to - 9, to, Moves::MoveFlag::CAPTURE));
        }

        // Quiet Push Promotions (Crucial for QS because they create a Queen out of nothing)
        auto singlePushPromo= (pawns << 8) & emptySquares & Bitboards::Rank8;
        while(singlePushPromo) {
            int to= Bitboards::PopLSB(singlePushPromo);
            moves.push_back(Moves::Encode(to - 8, to, Moves::MoveFlag::PROMO_QUEEN));
        }

        // En Passant
        if(board.enPassantIndex != -1) {
            int epSquare= board.enPassantIndex;
            if(((pawns & ~Bitboards::FileH) << 9) & (1ULL << epSquare)) {
                moves.push_back(Moves::Encode(epSquare - 9, epSquare, Moves::MoveFlag::EN_PASSANT));
            }
            if(((pawns & ~Bitboards::FileA) << 7) & (1ULL << epSquare)) {
                moves.push_back(Moves::Encode(epSquare - 7, epSquare, Moves::MoveFlag::EN_PASSANT));
            }
        }

    } else {
        // --- BLACK PAWNS ---
        auto captureRight= ((pawns & ~Bitboards::FileH) >> 7) & opponentOccupancy;
        auto captureLeft= ((pawns & ~Bitboards::FileA) >> 9) & opponentOccupancy;

        auto c1= captureLeft;
        while(c1) {
            int to= Bitboards::PopLSB(c1);
            if(to <= 7) {
                moves.push_back(Moves::Encode(to + 9, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_QUEEN));
            } else moves.push_back(Moves::Encode(to + 9, to, Moves::MoveFlag::CAPTURE));
        }

        auto c2= captureRight;
        while(c2) {
            int to= Bitboards::PopLSB(c2);
            if(to <= 7) {
                moves.push_back(Moves::Encode(to + 7, to, Moves::MoveFlag::CAPTURE | Moves::MoveFlag::PROMO_QUEEN));
            } else moves.push_back(Moves::Encode(to + 7, to, Moves::MoveFlag::CAPTURE));
        }

        auto singlePushPromo= (pawns >> 8) & emptySquares & Bitboards::Rank1;
        while(singlePushPromo) {
            int to= Bitboards::PopLSB(singlePushPromo);
            moves.push_back(Moves::Encode(to + 8, to, Moves::MoveFlag::PROMO_QUEEN));
        }

        if(board.enPassantIndex != -1) {
            int epSquare= board.enPassantIndex;
            if(((pawns & ~Bitboards::FileH) >> 7) & (1ULL << epSquare)) {
                moves.push_back(Moves::Encode(epSquare + 7, epSquare, Moves::MoveFlag::EN_PASSANT));
            }
            if(((pawns & ~Bitboards::FileA) >> 9) & (1ULL << epSquare)) {
                moves.push_back(Moves::Encode(epSquare + 9, epSquare, Moves::MoveFlag::EN_PASSANT));
            }
        }
    }
}

}; // namespace Knilb::MoveGeneration