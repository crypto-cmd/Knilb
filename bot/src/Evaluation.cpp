#include "Evaluation.hpp"
#include "Bitboard.hpp"
#include "Board.hpp"
#include "MoveGeneration.hpp"
#include "Piece.hpp"
#include <algorithm>
namespace Knilb::Engine::Evaluation {
static int EvaluatePawns(const Representation::Board& board) {
    using namespace Representation::Pieces;
    using namespace Knilb::Representation::Bitboards;

    uint64_t wPawns= board.GetBitBoard(WHITE | Kind::PAWN);
    uint64_t bPawns= board.GetBitBoard(BLACK | Kind::PAWN);

    int score= 0;

    // --- 1. ISOLATED PAWNS (O(1) Bitwise) ---
    // A pawn is isolated if there are no friendly pawns on adjacent files.

    uint64_t wAdjacent= ((wPawns & ~FileA) >> 1) | ((wPawns & ~FileH) << 1);
    uint64_t wIsolated= wPawns & ~wAdjacent;
    score-= 15 * CountBits(wIsolated);

    uint64_t bAdjacent= ((bPawns & ~FileA) >> 1) | ((bPawns & ~FileH) << 1);
    uint64_t bIsolated= bPawns & ~bAdjacent;
    score+= 15 * CountBits(bIsolated); // Positive score favors White

    // --- 2. PASSED PAWNS (Parallel Prefix Fill) ---

    // Fill Black pawns SOUTH to see every square they can block or attack
    uint64_t bSpan= bPawns >> 8;
    bSpan|= bSpan >> 16;
    bSpan|= bSpan >> 32;

    // Include adjacent files to represent Black's full pawn control span
    uint64_t bControl= bSpan | ((bSpan & ~FileA) >> 1) | ((bSpan & ~FileH) << 1);

    // Any White pawn that is NOT intersecting Black's control span is a passed pawn!
    uint64_t wPassed= wPawns & ~bControl;

    // We only loop over the passed pawns (usually 0 to 2 pawns) to apply the rank bonus
    while(wPassed) {
        int sq= PopLSB(wPassed);
        int rank= sq / 8;
        score+= 20 + (rank * rank);
    }

    // Fill White pawns NORTH to see every square they can block or attack
    uint64_t wSpan= wPawns << 8;
    wSpan|= wSpan << 16;
    wSpan|= wSpan << 32;

    // Include adjacent files for White's full pawn control span
    uint64_t wControl= wSpan | ((wSpan & ~FileA) >> 1) | ((wSpan & ~FileH) << 1);

    // Any Black pawn that is NOT intersecting White's control span is passed
    uint64_t bPassed= bPawns & ~wControl;

    while(bPassed) {
        int sq= PopLSB(bPassed);
        int rank= sq / 8;
        int passedRank= 7 - rank; // Distance from promotion
        score-= 20 + (passedRank * passedRank);
    }

    return score;
}

int EvaluateKingSafety(const Representation::Board& board, Pieces::Color color) {
    using namespace Representation::Pieces;
    using namespace MoveGeneration;
    using namespace Knilb::Representation::Bitboards;

    int penalty= 0;
    uint64_t kingSqBB= board.GetBitBoard(color | Kind::KING);
    if(!kingSqBB) return 0; // King is missing, should not happen in a valid position

    int ksq= GetLSBIndex(kingSqBB);

    auto myPawns= board.GetBitBoard(color | Kind::PAWN);
    auto oppColor= (color == WHITE) ? BLACK : WHITE;
    auto oppPawns= board.GetBitBoard(oppColor | Kind::PAWN);

    auto fileMask= FileA << (ksq % 8);

    auto myPawnsOnKFile= myPawns & fileMask != 0ULL;
    auto oppPawnsOnKFile= oppPawns & fileMask != 0ULL;

    if(!myPawnsOnKFile) {
        penalty+= (!oppPawnsOnKFile) ? 40 : 20; // No friendly pawns on the file is bad, but if opponent also has no pawns there it's even worse
    }

    // Pawn Shield
    auto kingRing= KingAttacks[ksq];
    auto shieldMask= color == WHITE ? (kingRing) << 8 : (kingRing) >> 8; // Squares in front of the king

    auto shieldPawns= myPawns & shieldMask;
    int shieldCount= CountBits(shieldPawns);
    penalty+= (3 - shieldCount) * 10; // Fewer pawns in the shield is worse

    return penalty;
}

static int EvaluateMopUp(const Representation::Board& board, Representation::Pieces::Color winningColor) {
    using namespace Representation::Pieces;
    using namespace Knilb::Representation::Bitboards;

    Color losingColor= (winningColor == WHITE) ? BLACK : WHITE;

    uint64_t wKingBB= board.GetBitBoard(winningColor | Kind::KING);
    uint64_t lKingBB= board.GetBitBoard(losingColor | Kind::KING);
    if(!wKingBB || !lKingBB) return 0;

    int wKSq= GetLSBIndex(wKingBB);
    int lKSq= GetLSBIndex(lKingBB);

    int wKRank= wKSq / 8;
    int wKFile= wKSq % 8;
    int lKRank= lKSq / 8;
    int lKFile= lKSq % 8;

    int score= 0;

    // 1. Reward pushing the losing king to the edges
    int centerDistX= std::max(3 - lKFile, lKFile - 4);
    int centerDistY= std::max(3 - lKRank, lKRank - 4);
    int centerDist= centerDistX + centerDistY;
    score+= centerDist * 10;

    // 2. Reward moving the friendly king close to the losing king
    int kingDist= std::max(std::abs(wKRank - lKRank), std::abs(wKFile - lKFile));
    score+= (14 - kingDist) * 4;

    // 3. Bishop + Knight specific logic (Orientation-Agnostic)
    uint64_t winningKnights= board.GetBitBoard(winningColor | Kind::KNIGHT);
    uint64_t winningBishops= board.GetBitBoard(winningColor | Kind::BISHOP);

    if(CountBits(winningKnights) == 1 && CountBits(winningBishops) == 1 &&
       board.GetBitBoard(winningColor | Kind::PAWN) == 0 &&
       board.GetBitBoard(winningColor | Kind::ROOK) == 0 &&
       board.GetBitBoard(winningColor | Kind::QUEEN) == 0) {

        int bSq= GetLSBIndex(winningBishops);

        // Find the color of the bishop using the engine's internal coordinates
        int bColor= ((bSq / 8) + (bSq % 8)) % 2;

        // Define the 4 corners in (file, rank) format
        int corners[4][2]= {{0, 0}, {7, 7}, {0, 7}, {7, 0}};
        int minDistToCorrectCorner= 14;

        for(int i= 0; i < 4; ++i) {
            int cornerFile= corners[i][0];
            int cornerRank= corners[i][1];

            // Check the color of this specific corner
            int cornerColor= (cornerFile + cornerRank) % 2;

            // If the corner matches the bishop's color, calculate distance to it
            if(cornerColor == bColor) {
                int dist= std::abs(lKFile - cornerFile) + std::abs(lKRank - cornerRank);
                if(dist < minDistToCorrectCorner) {
                    minDistToCorrectCorner= dist;
                }
            }
        }

        // Massive reward multiplier (60) to completely override the general centerDist logic
        // This ensures moving toward the correct corner is always the highest scoring move
        score+= (14 - minDistToCorrectCorner) * 60;
    }

    return score;
}

int evaluate(const Representation::Board& _board) {
    float phase= static_cast<float>(_board.getPhaseCounter()) / Knilb::Engine::Evaluation::MAX_PHASE;
    phase= std::clamp(phase, 0.0f, 1.0f);

    int materialAndPSTScore= static_cast<int>(_board.getOpeningScore() * phase + _board.getEndgameScore() * (1.0f - phase));

    // Integrate the pawn structure evaluation (this applies equally well in opening and endgame)
    int pawnStructureScore= EvaluatePawns(_board);

    auto whiteKingSafety= EvaluateKingSafety(_board, Representation::Pieces::Color::WHITE);
    auto blackKingSafety= EvaluateKingSafety(_board, Representation::Pieces::Color::BLACK);

    auto kingSafety= -whiteKingSafety + blackKingSafety; // Higher penalty means worse for White, so subtract White's penalty

    int score= materialAndPSTScore + pawnStructureScore + kingSafety;

    // --- MOP-UP INTEGRATION ---
    // Only apply mop-up logic in the endgame when pieces are off the board
    if(phase < 0.3f) {
        // If White is winning by at least a piece (+300) and Black has no pawns left
        if(score > 300 && _board.GetBitBoard(Representation::Pieces::BLACK | Representation::Pieces::Kind::PAWN) == 0) {
            score+= EvaluateMopUp(_board, Representation::Pieces::WHITE);
        }
        // If Black is winning by at least a piece (-300) and White has no pawns left
        else if(score < -300 && _board.GetBitBoard(Representation::Pieces::WHITE | Representation::Pieces::Kind::PAWN) == 0) {
            score-= EvaluateMopUp(_board, Representation::Pieces::BLACK); // Subtract because Black wants a more negative score
        }
    }

    int perspective= _board.activeColor == Representation::Pieces::Color::WHITE ? 1 : -1;
    return score * perspective;
}

// Helper function to find the Least Valuable Attacker (LVA) to a specific square
static int getLeastValuableAttacker(int targetSq, uint64_t occupied, Representation::Pieces::Color color, const Representation::Board& board, int& outPieceValue) {
    using namespace Representation::Pieces;
    using namespace Knilb::MoveGeneration;
    using namespace Knilb::Representation::Bitboards;

    uint64_t attackers= 0;

    // 1. Check for Pawn attackers
    uint64_t theirPawns= board.GetBitBoard(color | Kind::PAWN) & occupied;
    // To see which pawns attack the target, we look BACKWARDS using the opponent's pawn attack pattern
    int oppColorIdx= (color == WHITE) ? 1 : 0;
    attackers= PawnAttacks[oppColorIdx][targetSq] & theirPawns;
    if(attackers) {
        outPieceValue= GetPieceValue(Kind::PAWN);
        return GetLSBIndex(attackers); // Return first pawn found
    }

    // 2. Check for Knight attackers
    uint64_t theirKnights= board.GetBitBoard(color | Kind::KNIGHT) & occupied;
    attackers= KnightAttacks[targetSq] & theirKnights;
    if(attackers) {
        outPieceValue= GetPieceValue(Kind::KNIGHT);
        return GetLSBIndex(attackers);
    }

    // 3. Check for Bishop attackers
    uint64_t theirBishops= board.GetBitBoard(color | Kind::BISHOP) & occupied;
    attackers= GetBishopAttacks(targetSq, occupied) & theirBishops;
    if(attackers) {
        outPieceValue= GetPieceValue(Kind::BISHOP);
        return GetLSBIndex(attackers);
    }

    // 4. Check for Rook attackers
    uint64_t theirRooks= board.GetBitBoard(color | Kind::ROOK) & occupied;
    attackers= GetRookAttacks(targetSq, occupied) & theirRooks;
    if(attackers) {
        outPieceValue= GetPieceValue(Kind::ROOK);
        return GetLSBIndex(attackers);
    }

    // 5. Check for Queen attackers
    uint64_t theirQueens= board.GetBitBoard(color | Kind::QUEEN) & occupied;
    attackers= (GetBishopAttacks(targetSq, occupied) | GetRookAttacks(targetSq, occupied)) & theirQueens;
    if(attackers) {
        outPieceValue= GetPieceValue(Kind::QUEEN);
        return GetLSBIndex(attackers);
    }

    // 6. Check for King attackers
    uint64_t theirKing= board.GetBitBoard(color | Kind::KING) & occupied;
    attackers= KingAttacks[targetSq] & theirKing;
    if(attackers) {
        outPieceValue= GetPieceValue(Kind::KING);
        return GetLSBIndex(attackers);
    }

    return -1; // No attackers left
}

int SEE(const Representation::Board& board, Representation::Moves::Move move) {
    using namespace Representation::Moves;
    using namespace Representation::Pieces;

    int from= GetFrom(move);
    int to= GetTo(move);
    uint16_t flag= GetFlag(move);

    // If it's a quiet move (not a capture), material gain is 0
    if((flag & CAPTURE_MASK) == 0 && flag != MoveFlag::EN_PASSANT) {
        return 0;
    }

    int gain[32];
    int d= 0;

    // 1. Initial capture value
    Piece targetPiece= board.squares[to];
    int targetValue= GetPieceValue(targetPiece);

    if(flag == MoveFlag::EN_PASSANT) {
        targetValue= GetPieceValue(Kind::PAWN); // Target is physically on a different square, but the material gain is a pawn
    }
    gain[d]= targetValue;

    // 2. Value of the piece making the initial capture
    Piece attackerPiece= board.squares[from];
    int attackerValue= GetPieceValue(attackerPiece);

    if(flag & PROMOTION_MASK) {
        if(flag == MoveFlag::PROMO_QUEEN || flag == MoveFlag::CAPTURE_PROMO_QUEEN) attackerValue= GetPieceValue(Kind::QUEEN);
        else if(flag == MoveFlag::PROMO_ROOK || flag == MoveFlag::CAPTURE_PROMO_ROOK) attackerValue= GetPieceValue(Kind::ROOK);
        else if(flag == MoveFlag::PROMO_BISHOP || flag == MoveFlag::CAPTURE_PROMO_BISHOP) attackerValue= GetPieceValue(Kind::BISHOP);
        else if(flag == MoveFlag::PROMO_KNIGHT || flag == MoveFlag::CAPTURE_PROMO_KNIGHT) attackerValue= GetPieceValue(Kind::KNIGHT);
    }

    // 3. Setup occupied bitboard to unmask X-Ray attackers during the sequence
    uint64_t occupied= board.GetBitBoard(WHITE) | board.GetBitBoard(BLACK);

    // Simulate the first move
    occupied&= ~(1ULL << from); // Remove initial attacker from occupancy
    occupied|= (1ULL << to);    // Place it on target square (to block x-rays going THROUGH the target)

    Color nextColor= (board.activeColor == WHITE) ? BLACK : WHITE;

    // 4. Main sequence loop
    while(true) {
        d++;
        int currentAttackerValue= 0;

        // Find the next least valuable piece that can attack the target square
        int attackerSq= getLeastValuableAttacker(to, occupied, nextColor, board, currentAttackerValue);

        if(attackerSq == -1) break; // No more attackers

        // Record the material swing for this capture
        gain[d]= attackerValue - gain[d - 1];

        // Prepare for next iteration
        occupied&= ~(1ULL << attackerSq); // Remove this attacker to expose X-Rays behind it
        attackerValue= currentAttackerValue;
        nextColor= (nextColor == WHITE) ? BLACK : WHITE;
    }

    // 5. Minimax backward pass to evaluate the best sequence for both sides
    while(--d > 0) {
        gain[d - 1]= -std::max(-gain[d - 1], gain[d]);
    }

    // Returns > 0 if the capture wins material, < 0 if it loses material, 0 if equal
    return gain[0];
}

} // namespace Knilb::Engine::Evaluation