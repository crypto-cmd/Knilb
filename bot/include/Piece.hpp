#pragma once
#include <cstdint>
namespace Knilb::Representation::Pieces {

enum Kind {
    NONE= 0,
    PAWN= 0b001,
    KNIGHT= 0b010,
    BISHOP= 0b011,
    ROOK= 0b100,
    QUEEN= 0b101,
    KING= 0b110
};
constexpr uint8_t PieceKindMask= 0b111;

enum Color {
    NO_COLOR= 0,
    WHITE= 0b01000,
    BLACK= 0b10000,
};
constexpr uint8_t PieceColorMask= 0b11000;

using Piece= uint8_t; // [Color: 2 bits][Kind: 3 bits]
inline Color GetColor(Piece piece) {
    return static_cast<Color>(piece & PieceColorMask);
}
inline bool IsColor(Piece piece, Color color) {
    return GetColor(piece) == color;
}

inline Kind GetKind(Piece piece) {
    return static_cast<Kind>(piece & PieceKindMask);
}

inline bool IsKind(Piece piece, Kind type) {
    return GetKind(piece) == type;
}
constexpr char GetSymbol(Piece piece) {
    switch(piece) {
    case NONE:
        return '.';
    case WHITE | PAWN:
        return 'P';
    case WHITE | KNIGHT:
        return 'N';
    case WHITE | BISHOP:
        return 'B';
    case WHITE | ROOK:
        return 'R';
    case WHITE | QUEEN:
        return 'Q';
    case WHITE | KING:
        return 'K';
    case BLACK | PAWN:
        return 'p';
    case BLACK | KNIGHT:
        return 'n';
    case BLACK | BISHOP:
        return 'b';
    case BLACK | ROOK:
        return 'r';
    case BLACK | QUEEN:
        return 'q';
    case BLACK | KING:
        return 'k';
    default:
        return '?';
    }
}
inline constexpr Piece FromSymbol(char symbol) {
    switch(symbol) {
    case '.': return NONE;
    case 'P': return WHITE | PAWN;
    case 'N': return WHITE | KNIGHT;
    case 'B': return WHITE | BISHOP;
    case 'R': return WHITE | ROOK;
    case 'Q': return WHITE | QUEEN;
    case 'K': return WHITE | KING;
    case 'p': return BLACK | PAWN;
    case 'n': return BLACK | KNIGHT;
    case 'b': return BLACK | BISHOP;
    case 'r': return BLACK | ROOK;
    case 'q': return BLACK | QUEEN;
    case 'k': return BLACK | KING;
    default: return NONE;
    }
}
} // namespace Knilb::Representation::Pieces
