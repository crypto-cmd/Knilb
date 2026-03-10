#include "Board.hpp"
#include "Coordinate.hpp"
#include "Evaluation.hpp"
#include "MoveGeneration.hpp"
#include "PieceSquareTable.hpp"
#include "Precomputed.hpp"
#include "Zobrist.hpp"
#include <algorithm>
#include <iostream>
#include <random>
#include <sstream>

namespace Knilb::Representation {

constexpr int getPieceIndex(Pieces::Piece piece) {
    if(piece == Pieces::Kind::NONE) return -1;
    auto kind= Pieces::GetKind(piece);
    int idx= 0;
    switch(kind) {
    case Pieces::Kind::PAWN: idx= 0; break;
    case Pieces::Kind::KNIGHT: idx= 1; break;
    case Pieces::Kind::BISHOP: idx= 2; break;
    case Pieces::Kind::ROOK: idx= 3; break;
    case Pieces::Kind::QUEEN: idx= 4; break;
    case Pieces::Kind::KING: idx= 5; break;
    default: return -1;
    }
    if(Pieces::IsColor(piece, Pieces::BLACK)) idx+= 6;
    return idx;
}

Board::Board() {
    setFen(STARTING_POS);
}

uint64_t Board::calculateHash() const {
    uint64_t hash= 0;
    for(int i= 0; i < 64; ++i) {
        if(squares[i] != Pieces::Kind::NONE) {
            hash^= Precomputed::Zobrist::Tables.pieces[getPieceIndex(squares[i])][i];
        }
    }
    hash^= Precomputed::Zobrist::Tables.castling[castlingRights];
    if(enPassantIndex != -1) hash^= Precomputed::Zobrist::Tables.enPassant[enPassantIndex];
    else hash^= Precomputed::Zobrist::Tables.enPassant[64];

    if(activeColor == Pieces::Color::BLACK) hash^= Precomputed::Zobrist::Tables.sideToMove;
    return hash;
}

void Board::setFen(const std::string& fen) {
    std::fill(std::begin(squares), std::end(squares), Pieces::Kind::NONE);
    std::fill(std::begin(bitboards), std::end(bitboards), 0ULL);
    historyIndex= 0;

    std::stringstream ss(fen);
    std::string boardPart, activeColorStr, castlingPart, enPassantPart;
    std::string halfMoveStr, fullMoveStr;

    ss >> boardPart >> activeColorStr >> castlingPart >> enPassantPart >> halfMoveStr >> fullMoveStr;

    int coord= Coordinates::A8; // Start from top-left corner and fill row by row
    for(char c: boardPart) {
        if(c == '/') {
            coord-= 16; // Move down one rank (8 squares) and back to file A (-8 squares)
            continue;
        }
        if(std::isdigit(c)) {
            coord+= (c - '0'); // Skip empty squares
        } else {

            Pieces::Piece piece= Pieces::FromSymbol(c);
            squares[coord]= piece;
            Bitboards::SetBit(bitboards[getPieceIndex(piece)], coord);
            // Move to the next file
            coord+= 1;
        }
    }

    this->activeColor= (activeColorStr == "w") ? Pieces::Color::WHITE : Pieces::Color::BLACK;

    this->castlingRights= 0;
    if(castlingPart != "-") {
        for(char c: castlingPart) {
            if(c == 'K') this->castlingRights|= CASTLE_WK;
            else if(c == 'Q') this->castlingRights|= CASTLE_WQ;
            else if(c == 'k') this->castlingRights|= CASTLE_BK;
            else if(c == 'q') this->castlingRights|= CASTLE_BQ;
        }
    }

    if(enPassantPart != "-") {
        this->enPassantIndex= Coordinates::FromAlgebraic(enPassantPart.c_str());
    } else {
        this->enPassantIndex= -1;
    }

    try {
        this->halfMoveClock= std::stoi(halfMoveStr);
        this->fullMoveNumber= std::stoi(fullMoveStr);
    } catch(...) {
        this->halfMoveClock= 0;
        this->fullMoveNumber= 1;
    }

    // Find Kings' positions
    for(int i= 0; i < 64; ++i) {
        Pieces::Piece piece= squares[i];
        if(Pieces::IsKind(piece, Pieces::Kind::KING)) {
            Coordinates::Coordinate pos= i;
            if(Pieces::IsColor(piece, Pieces::Color::WHITE)) {
                whiteKingPos= pos;
            } else {
                blackKingPos= pos;
            }
        }
    }
    updateOccupancies();
    this->zobristHash= calculateHash();

    // Initialize running evaluation information
    openingScore= 0;
    endgameScore= 0;
    phaseCounter= 0;

    for(int i= 0; i < 64; ++i) {
        auto piece= squares[i];
        if(piece == Representation::Pieces::Kind::NONE) continue;

        int colorMul= Representation::Pieces::IsColor(piece, Representation::Pieces::WHITE) ? 1 : -1;
        int pstOpening= Engine::Evaluation::PST::MiddleGamePieceSquareTable[Representation::Pieces::GetKind(piece)][Representation::Pieces::IsColor(piece, Representation::Pieces::Color::WHITE) ? i : Representation::Coordinates::Flip(i)];
        int pstEndgame= Engine::Evaluation::PST::EndGamePieceSquareTable[Representation::Pieces::GetKind(piece)][Representation::Pieces::IsColor(piece, Representation::Pieces::Color::WHITE) ? i : Representation::Coordinates::Flip(i)];
        openingScore+= colorMul * pstOpening;
        endgameScore+= colorMul * pstEndgame;

        // adjust phase counter for every non‑pawn, non‑king piece
        phaseCounter+= Engine::Evaluation::GetPhaseWeight(piece);
    }
    phaseCounter= std::clamp(phaseCounter, 0, Knilb::Engine::Evaluation::MAX_PHASE);
}

void Board::makeMove(const Moves::Move& move) {
    // 1. Extract move details
    int from= Moves::GetFrom(move);
    int to= Moves::GetTo(move);
    auto flag= Moves::GetFlag(move);

    bool isEnPassant= flag == Moves::MoveFlag::EN_PASSANT;
    bool isCastling= flag == Moves::MoveFlag::KING_CASTLE || flag == Moves::MoveFlag::QUEEN_CASTLE;
    bool isPromotion= flag & Moves::PROMOTION_MASK;
    auto isCapture= flag & Moves::CAPTURE_MASK;

    Pieces::Kind promoType= Pieces::Kind::NONE;
    if(isPromotion) {
        switch(flag) {
        case Moves::MoveFlag::PROMO_KNIGHT:
        case Moves::MoveFlag::CAPTURE_PROMO_KNIGHT:
            promoType= Pieces::Kind::KNIGHT;
            break;
        case Moves::MoveFlag::PROMO_BISHOP:
        case Moves::MoveFlag::CAPTURE_PROMO_BISHOP:
            promoType= Pieces::Kind::BISHOP;
            break;
        case Moves::MoveFlag::PROMO_ROOK:
        case Moves::MoveFlag::CAPTURE_PROMO_ROOK:
            promoType= Pieces::Kind::ROOK;
            break;
        case Moves::MoveFlag::PROMO_QUEEN:
        case Moves::MoveFlag::CAPTURE_PROMO_QUEEN:
            promoType= Pieces::Kind::QUEEN;
            break;
        }
    }
    Pieces::Piece originalPiece= squares[from];
    Pieces::Piece placedPiece= isPromotion ? (activeColor | promoType) : originalPiece;
    Pieces::Kind kind= Pieces::GetKind(originalPiece);

    int direction= (activeColor == Pieces::Color::WHITE) ? Precomputed::Direction::Offset::SOUTH : Precomputed::Direction::Offset::NORTH;
    int capIdx= isEnPassant ? to + direction : to;

    auto capturedPiece= squares[capIdx];

    // 2. Save History
    GameState state;
    state.move= move;
    state.capturedPiece= capturedPiece;
    state.castlingRights= castlingRights;
    state.enPassantIndex= enPassantIndex;
    state.halfMoveClock= halfMoveClock;
    state.zobristHash= zobristHash;
    state.whiteKingPos= whiteKingPos;
    state.blackKingPos= blackKingPos;

    // save the incremental evaluation values so undo can restore them directly

    state.openingScore= openingScore;
    state.endgameScore= endgameScore;
    state.phaseCounter= phaseCounter;

    game_history[historyIndex++]= state;

    int colorMultiplier= Representation::Pieces::IsColor(originalPiece, Representation::Pieces::WHITE) ? 1 : -1;

    // 3. Update Main Piece Squares
    squares[capIdx]= Pieces::Kind::NONE; // Remove captured piece (if any)
    if(capturedPiece != Pieces::Kind::NONE)
        Bitboards::PopBit(bitboards[getPieceIndex(capturedPiece)], capIdx);

    squares[to]= placedPiece; // Place moving piece (or promotion piece) on destination
    Bitboards::SetBit(bitboards[getPieceIndex(placedPiece)], to);

    squares[from]= Pieces::Kind::NONE;
    Bitboards::PopBit(bitboards[getPieceIndex(originalPiece)], from);

    // 4. Remove old Zobrist info BEFORE making changes
    zobristHash^= Precomputed::Zobrist::Tables.castling[castlingRights];
    zobristHash^= Precomputed::Zobrist::Tables.enPassant[enPassantIndex != -1 ? enPassantIndex : 64];

    // 5. Handle Capture (for Zobrist)
    if(isCapture) zobristHash^= Precomputed::Zobrist::Tables.pieces[getPieceIndex(capturedPiece)][capIdx];

    // 6. Update Kings' positions if needed
    if(kind == Pieces::Kind::KING) {
        if(Pieces::IsColor(placedPiece, Pieces::Color::WHITE)) whiteKingPos= to;
        else blackKingPos= to;
    }

    // 7. Handle Special Castling Rook Move
    if(isCastling) {
        auto isKingSide= flag == Moves::MoveFlag::KING_CASTLE;
        auto rookFrom= isKingSide ? from + 3 : from - 4;
        auto rookTo= isKingSide ? from + 1 : from - 1;

        // Update Rook on Board
        Pieces::Piece rook= squares[rookFrom];
        squares[rookTo]= rook;
        squares[rookFrom]= Pieces::Kind::NONE;

        // Update Rook in Bitboard
        Bitboards::PopBit(bitboards[getPieceIndex(rook)], rookFrom);
        Bitboards::SetBit(bitboards[getPieceIndex(rook)], rookTo);

        // Update Zobrist for Rook Move
        zobristHash^= Precomputed::Zobrist::Tables.pieces[getPieceIndex(rook)][rookFrom]; // Remove rook from original square
        zobristHash^= Precomputed::Zobrist::Tables.pieces[getPieceIndex(rook)][rookTo];   // Add rook to new square
    }

    // 8. Update Castling Rights
    if(kind == Pieces::Kind::KING) {
        if(activeColor == Pieces::Color::WHITE) castlingRights&= ~(CASTLE_WK | CASTLE_WQ);
        else castlingRights&= ~(CASTLE_BK | CASTLE_BQ);
    }
    if(from == Coordinates::A1 || to == Coordinates::A1) castlingRights&= ~CASTLE_WQ; // Rook a1 moved or captured
    if(from == Coordinates::H1 || to == Coordinates::H1) castlingRights&= ~CASTLE_WK; // Rook h1 moved or captured
    if(from == Coordinates::A8 || to == Coordinates::A8) castlingRights&= ~CASTLE_BQ; // Rook a8 moved or captured
    if(from == Coordinates::H8 || to == Coordinates::H8) castlingRights&= ~CASTLE_BK; // Rook h8 moved or captured

    // 9. Update En Passant Target
    enPassantIndex= -1; // Reset en passant by default
    if(flag == Moves::MoveFlag::DOUBLE_PAWN_PUSH) {
        auto oppColor= (activeColor == Pieces::Color::WHITE) ? Pieces::Color::BLACK : Pieces::Color::WHITE;
        // Check if opponent has a pawn beside the destination square
        int file= Coordinates::GetFile(to);
        bool isOpponentPawnBesideMe= false;

        if(!Coordinates::IsInFile(file, Coordinates::A_FILE)) {
            int leftSquare= to - 1;
            auto piece= squares[to - 1];
            if(piece != Pieces::Kind::NONE && Pieces::IsColor(piece, oppColor) && Pieces::IsKind(piece, Pieces::Kind::PAWN)) {
                isOpponentPawnBesideMe= true;
            }
        }
        if(!Coordinates::IsInFile(file, Coordinates::H_FILE)) {
            int rightSquare= to + 1;
            auto piece= squares[to + 1];
            if(piece != Pieces::Kind::NONE && Pieces::IsColor(piece, oppColor) && Pieces::IsKind(piece, Pieces::Kind::PAWN)) {
                isOpponentPawnBesideMe= true;
            }
        }
        enPassantIndex= isOpponentPawnBesideMe ? ((from + to) / 2) : -1;
    }

    // 10. Finalize Zobrist State
    zobristHash^= Precomputed::Zobrist::Tables.pieces[getPieceIndex(originalPiece)][from];            // Remove piece from source
    zobristHash^= Precomputed::Zobrist::Tables.pieces[getPieceIndex(placedPiece)][to];                // Add piece to destination
    zobristHash^= Precomputed::Zobrist::Tables.castling[castlingRights];                              // Add new castling rights
    zobristHash^= Precomputed::Zobrist::Tables.enPassant[enPassantIndex != -1 ? enPassantIndex : 64]; // Add new en passant state
    zobristHash^= Precomputed::Zobrist::Tables.sideToMove;                                            // Toggle side to move

    if(kind == Pieces::Kind::PAWN || capturedPiece != Pieces::Kind::NONE) halfMoveClock= 0;
    else halfMoveClock++;

    if(activeColor == Pieces::Color::BLACK) fullMoveNumber++;
    activeColor= (activeColor == Pieces::Color::WHITE) ? Pieces::Color::BLACK : Pieces::Color::WHITE;

    updateOccupancies();

    // --- incremental evaluation adjustments ---
    // Remove PST value from the old square (from)
    int colorMul= Representation::Pieces::IsColor(originalPiece, Representation::Pieces::WHITE) ? 1 : -1;
    int fromOpening= Engine::Evaluation::PST::MiddleGamePieceSquareTable[kind][colorMul == 1 ? from : Representation::Coordinates::Flip(from)];
    int fromEndgame= Engine::Evaluation::PST::EndGamePieceSquareTable[kind][colorMul == 1 ? from : Representation::Coordinates::Flip(from)];
    openingScore-= colorMul * fromOpening;
    endgameScore-= colorMul * fromEndgame;

    // Add PST value to the new square (to)
    int placedKind= Representation::Pieces::GetKind(placedPiece);
    int toOpening= Engine::Evaluation::PST::MiddleGamePieceSquareTable[placedKind][colorMul == 1 ? to : Representation::Coordinates::Flip(to)];
    int toEndgame= Engine::Evaluation::PST::EndGamePieceSquareTable[placedKind][colorMul == 1 ? to : Representation::Coordinates::Flip(to)];
    openingScore+= colorMul * toOpening;
    endgameScore+= colorMul * toEndgame;

    // if castling, also move the rook's PST values
    if(isCastling) {
        int rookColorMul= colorMul;
        int rookKind= Representation::Pieces::Kind::ROOK;
        int rookFrom= (flag == Moves::MoveFlag::KING_CASTLE) ? from + 3 : from - 4;
        int rookTo= (flag == Moves::MoveFlag::KING_CASTLE) ? from + 1 : from - 1;
        int rFromO= Engine::Evaluation::PST::MiddleGamePieceSquareTable[rookKind][rookColorMul == 1 ? rookFrom : Representation::Coordinates::Flip(rookFrom)];
        int rFromE= Engine::Evaluation::PST::EndGamePieceSquareTable[rookKind][rookColorMul == 1 ? rookFrom : Representation::Coordinates::Flip(rookFrom)];
        int rToO= Engine::Evaluation::PST::MiddleGamePieceSquareTable[rookKind][rookColorMul == 1 ? rookTo : Representation::Coordinates::Flip(rookTo)];
        int rToE= Engine::Evaluation::PST::EndGamePieceSquareTable[rookKind][rookColorMul == 1 ? rookTo : Representation::Coordinates::Flip(rookTo)];
        openingScore-= rookColorMul * rFromO;
        endgameScore-= rookColorMul * rFromE;
        openingScore+= rookColorMul * rToO;
        endgameScore+= rookColorMul * rToE;
    }

    // Captures
    if(capturedPiece != Pieces::Kind::NONE) {
        int cm= Representation::Pieces::IsColor(capturedPiece, Representation::Pieces::WHITE) ? 1 : -1;
        int capKind= Representation::Pieces::GetKind(capturedPiece);
        openingScore-= cm * Engine::Evaluation::PST::MiddleGamePieceSquareTable[capKind][cm == 1 ? capIdx : Representation::Coordinates::Flip(capIdx)];
        endgameScore-= cm * Engine::Evaluation::PST::EndGamePieceSquareTable[capKind][cm == 1 ? capIdx : Representation::Coordinates::Flip(capIdx)];

        // Subtract captured piece weight
        phaseCounter-= Engine::Evaluation::GetPhaseWeight(capturedPiece);
    }

    // Promotions
    if(isPromotion) {
        // ADD promoted piece weight (Queens increase phase complexity)
        phaseCounter+= Engine::Evaluation::GetPhaseWeight(placedPiece);
    }

    phaseCounter= std::clamp(phaseCounter, 0, Knilb::Engine::Evaluation::MAX_PHASE);
}
bool Board::isInCheck() {
    auto oppKingPos= (activeColor == Pieces::WHITE) ? blackKingPos : whiteKingPos;
    auto myColor= activeColor;
    Pieces::Color oppColor= (activeColor == Pieces::WHITE) ? Pieces::BLACK : Pieces::WHITE;
    Coordinates::Coordinate myKingPos= (activeColor == Pieces::WHITE) ? whiteKingPos : blackKingPos;
    return MoveGeneration::IsSquareAttacked(*this, myKingPos, oppColor);
}

void Board::undoMove() {
    if(historyIndex == 0) return;

    // 1. Restore Board State
    GameState lastState= game_history[--historyIndex];

    activeColor= (activeColor == Pieces::Color::WHITE) ? Pieces::Color::BLACK : Pieces::Color::WHITE;
    if(activeColor == Pieces::Color::BLACK) fullMoveNumber--;

    auto capturedPiece= lastState.capturedPiece;

    // 1. Restore the originally moved piece to its source square
    const Moves::Move& move= lastState.move;
    auto to= Moves::GetTo(move);
    auto from= Moves::GetFrom(move);

    auto flag= Moves::GetFlag(move);
    bool isPromotion= flag & Moves::PROMOTION_MASK;
    bool isCastling= flag == Moves::MoveFlag::KING_CASTLE || flag == Moves::MoveFlag::QUEEN_CASTLE;
    bool isEnPassant= flag == Moves::MoveFlag::EN_PASSANT;

    // Capture the piece at destination before modifications (for bitboard update)
    Pieces::Piece pieceOnTo= squares[to];
    Bitboards::PopBit(bitboards[getPieceIndex(pieceOnTo)], to);

    if(isPromotion) {
        // The piece on the destination square is the promoted piece.
        // We overwrite the source square with a standard Pawn.
        Pieces::Piece originalPawn= activeColor | Pieces::PAWN;
        squares[from]= originalPawn;
        Bitboards::SetBit(bitboards[getPieceIndex(originalPawn)], from);
    } else {
        squares[from]= pieceOnTo;
        Bitboards::SetBit(bitboards[getPieceIndex(pieceOnTo)], from);
    }

    if(isEnPassant) {
        squares[to]= Pieces::Kind::NONE;

        // The captured pawn is exactly on the destination's file, but the source's rank.
        int capFile= Coordinates::GetFile(to);
        int capRank= Coordinates::GetRank(from);
        int capIdx= Coordinates::Create(capFile, capRank);

        squares[capIdx]= capturedPiece;
        Bitboards::SetBit(bitboards[getPieceIndex(capturedPiece)], capIdx);
    } else {
        squares[to]= capturedPiece; // Restore captured piece (if any) to destination square
        if(capturedPiece != Pieces::Kind::NONE) {
            Bitboards::SetBit(bitboards[getPieceIndex(capturedPiece)], to);
        }
    }

    if(isCastling) {
        int rookFromIdx= (flag == Moves::MoveFlag::KING_CASTLE) ? from + 3 : from - 4;
        int rookToIdx= (flag == Moves::MoveFlag::KING_CASTLE) ? from + 1 : from - 1;

        Pieces::Piece rook= squares[rookToIdx];
        squares[rookFromIdx]= rook;
        squares[rookToIdx]= Pieces::Kind::NONE;
        Bitboards::PopBit(bitboards[getPieceIndex(rook)], rookToIdx);
        Bitboards::SetBit(bitboards[getPieceIndex(rook)], rookFromIdx);
    }

    castlingRights= lastState.castlingRights;
    enPassantIndex= lastState.enPassantIndex;
    halfMoveClock= lastState.halfMoveClock;
    zobristHash= lastState.zobristHash;
    whiteKingPos= lastState.whiteKingPos;
    blackKingPos= lastState.blackKingPos;

    updateOccupancies();
    // restore incremental evaluation values from history
    openingScore= lastState.openingScore;
    endgameScore= lastState.endgameScore;
    phaseCounter= lastState.phaseCounter;
}

void Board::print() const {
    std::cout << "\n  +-----------------+\n";
    for(int rank= 7; rank >= 0; --rank) {
        std::cout << rank + 1 << " | ";
        for(int file= 0; file < 8; ++file) {
            int square= rank * 8 + file;
            Pieces::Piece p= squares[square];
            char c= p != Pieces::Kind::NONE ? Pieces::GetSymbol(p) : '.';
            std::cout << c << " ";
        }
        std::cout << "|\n";
    }
    std::cout << "  +-----------------+\n";
    std::cout << "    a b c d e f g h\n\n";
    std::cout << "Zobrist Hash: " << std::hex << zobristHash << std::dec << "\n";

    // Print each bitboard for debugging
    const char* pieceNames[12]= {"WP", "WN", "WB", "WR", "WQ", "WK", "BP", "BN", "BB", "BR", "BQ", "BK"};
    for(int i= 0; i < 12; ++i) {
        std::cout << pieceNames[i] << " Bitboard:\n";
        for(int rank= 7; rank >= 0; --rank) {
            for(int file= 0; file < 8; ++file) {
                int square= rank * 8 + file;
                std::cout << ((bitboards[i] & (1ULL << square)) ? "1 " : ". ");
            }
            std::cout << "\n";
        }
    }
}
} // namespace Knilb::Representation