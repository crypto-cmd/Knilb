#include "Bot.hpp"
#include "Board.hpp"
#include "Coordinate.hpp"
#include "Evaluation.hpp"
#include "Search.hpp"
#include "Zobrist.hpp"
#include <algorithm>
#include <chrono>

namespace Knilb::Engine {
using namespace Representation;
using namespace MoveGeneration;
using namespace Representation::Pieces;

// --- Search Enhancements ---
bool Bot::tryNullMovePruning(int depth, int ply, int alpha, int beta) {
    int R= 3 + (depth / 6);
    if(depth < 3 || _board.isTwofoldRepetition()) return false;

    auto myColor= _board.activeColor;
    auto oppColor= (myColor == Pieces::WHITE) ? Pieces::BLACK : Pieces::WHITE;
    auto myKingPos= (myColor == Pieces::WHITE) ? _board.whiteKingPos : _board.blackKingPos;

    // Check restriction
    if(MoveGeneration::IsSquareAttacked(_board, myKingPos, oppColor)) return false;

    // Only check side to move for non-pawn material
    auto allMaterial= _board.GetBitBoard(_board.activeColor);
    auto pawns= _board.GetBitBoard(_board.activeColor | Pieces::Kind::PAWN);
    auto nonPawnMaterial= allMaterial & ~pawns;
    bool hasNonPawnMaterial= nonPawnMaterial != 0ULL;

    if(!hasNonPawnMaterial) return false;

    // Make the Null Move (Save state, toggle color, clear EP, update Zobrist)
    // It is highly recommended to build this into a _board.makeNullMove() function.
    int savedEP= _board.enPassantIndex;
    uint64_t savedHash= _board.zobristHash;

    _board.activeColor= oppColor;
    if(savedEP != -1) {
        _board.zobristHash^= Precomputed::Zobrist::Tables.enPassant[savedEP];
        _board.zobristHash^= Precomputed::Zobrist::Tables.enPassant[64]; // Add this line
        _board.enPassantIndex= -1;
    }
    _board.zobristHash^= Precomputed::Zobrist::Tables.sideToMove; // Toggle side to move in hash

    // Zero-Window Search
    int score= -search(depth - 1 - R, ply + 1, -beta, -beta + 1);

    // Unmake the Null Move (Restore exact state)
    _board.activeColor= myColor;
    _board.enPassantIndex= savedEP;
    _board.zobristHash= savedHash;

    // Cutoff check with Verification Search
    if(score >= beta) {
        // If the depth is high enough, Zugzwang false cutoffs are highly damaging.
        // We do a shallow search with the normal side-to-move to verify the cutoff is real.
        if(depth > 4) {
            int verificationDepth= depth - 4;
            // Use a zero-window around beta to prove we are still failing high
            int verificationScore= search(verificationDepth, ply, beta - 1, beta);

            if(verificationScore < beta) {
                return false; // False cutoff (Zugzwang detected). Do not prune.
            }
        }
        return true; // Verified cutoff
    }
    return false;
}

// Late Move Reduction
int Bot::GetLateMoveReduction(int depth, int moveIndex, bool isPVNode, bool isCapture, bool isCheck) {
    // Only reduce for quiet moves, not in PV, not captures, not checks
    if(isPVNode || isCapture || isCheck || depth < 3 || moveIndex < 3) return 0;

    int safeDepth= depth > 63 ? 63 : depth;
    int safeMoveIndex= (moveIndex + 1) > 63 ? 63 : (moveIndex + 1);
    int reduction= Search::LMR::Table[safeDepth][safeMoveIndex];
    return reduction > 0 ? reduction : 0;
}

// Extensions
int Bot::GetExtension(const Representation::Board& board, const Representation::Moves::Move& move, int depth, bool isInCheck) {
    if(depth < 5) return 0; // Only consider extensions at deeper depths
    if(isInCheck) return 1;
    return 0;
}

Bot::Bot(): _board(), _tt(4 * 1024) {
}

void Bot::setFen(const std::string& fen) {
    _board.setFen(fen);
}
void Bot::PerformMove(const std::string& moveStr) {
    // Convert moveStr (e.g., "e2e4") to a Move object
    MoveGeneration::MoveList moves;
    MoveGeneration::Generator::GenerateMoves(_board, moves);
    for(const auto& move: moves) {
        if(Representation::Moves::toString(move) != moveStr) {
            continue; // Not the move we're looking for
        }
        _board.makeMove(move);

        // Determine legality after making the move (e.g., not leaving king in check)
        if(!MoveGeneration::IsLegalPosition(_board)) {
            _board.undoMove(); // Illegal move, undo it
            std::cerr << "Illegal move attempted: " << moveStr << std::endl;
        }
        return;
    }
    throw std::invalid_argument("Invalid move string: " + moveStr);
}
// Probes the TT. Returns true if a cutoff occurred, updating ttScore and ttBestMove.
bool Bot::ProbeTT(int depth, int ply, int alpha, int beta, int& ttScore, Representation::Moves::Move& ttBestMove) {
    _stats.ttProbes++;
    auto& ttEntry= _tt.probe(_board.zobristHash);

    // 1. Hash mismatch: Not the same position
    if(ttEntry.zobristHash != _board.zobristHash) return false;

    ttBestMove= ttEntry.bestMove;
    _stats.ttHits++;

    // 2. Depth check: In QS, depth is 0, so this condition safely passes everything
    if(ttEntry.depth >= depth) {
        int score= ttEntry.score;

        // Convert mate score from distance-to-node back to distance-to-root
        if(score > Evaluation::MATE_VAL - 100) score-= ply;
        else if(score < -Evaluation::MATE_VAL + 100) score+= ply;

        // 3. Bound checking for cutoffs
        if(ttEntry.flag == Search::TTFlag::TT_EXACT) {
            ttScore= score;
            _stats.ttCutoffs++;
            return true;
        }
        if(ttEntry.flag == Search::TTFlag::TT_ALPHA && score <= alpha) {
            ttScore= alpha;
            _stats.ttCutoffs++;
            return true;
        }
        if(ttEntry.flag == Search::TTFlag::TT_BETA && score >= beta) {
            ttScore= beta;
            _stats.ttCutoffs++;
            return true;
        }
    }
    return false; // Valid TT entry, but no cutoff occurred
}
// Stores data into the TT, handling mate score conversions and fail-low preservation
void Bot::StoreTT(int depth, int ply, int bestScore, int originalAlpha, int beta, Representation::Moves::Move bestMove) {
    int storedScore= bestScore;

    // Convert mate score from distance-to-root to distance-to-node
    if(storedScore > Evaluation::MATE_VAL - 100) storedScore+= ply;
    else if(storedScore < -Evaluation::MATE_VAL + 100) storedScore-= ply;

    Search::TTFlag flag;
    if(bestScore <= originalAlpha) flag= Search::TTFlag::TT_ALPHA; // Fail-Low
    else if(bestScore >= beta) flag= Search::TTFlag::TT_BETA;      // Fail-High
    else flag= Search::TTFlag::TT_EXACT;                           // PV Node

    auto& ttEntry= _tt.probe(_board.zobristHash);

    auto isSameHash= (ttEntry.zobristHash == _board.zobristHash);
    if(!isSameHash) {
        // PV protection
        if(ttEntry.flag == Search::TTFlag::TT_EXACT && flag == Search::TTFlag::TT_EXACT && ttEntry.depth >= depth) {
            // Preserving a deeper exact entry from being overwritten by a shallower one
            return; // Do not store the new entry
        }

        if(ttEntry.depth >= depth)
            return;
    }

    Representation::Moves::Move moveToStore= bestMove;

    // Preserve historical move on Fail-Low
    if(bestScore <= originalAlpha) {
        if(ttEntry.zobristHash == _board.zobristHash) {
            moveToStore= ttEntry.bestMove;
        } else {
            moveToStore= Representation::Moves::DEFAULT_MOVE;
        }
    }

    Search::TTEntry replacementEntry;
    replacementEntry.zobristHash= _board.zobristHash;
    replacementEntry.score= storedScore;
    replacementEntry.depth= depth;
    replacementEntry.flag= flag;
    replacementEntry.bestMove= moveToStore;

    _tt.store(replacementEntry);
}
int Bot::search(int depth, int ply, int alpha, int beta) {

    _stats.positionsEvaluated++;
    // 1. Check for interruptions every 512 nodes (more responsive to stop command)
    if((_stats.positionsEvaluated & 511) == 0) {
        if((_stopFlag && _stopFlag->load(std::memory_order_relaxed)) || IsFinishThinking()) {
            _searchAborted= true;
        }
    }

    // 2. Don't search if we've been signaled to stop
    if(_searchAborted) {
        return 0;
    }

    // Optimized twofold repetition detection (only at root)
    if(ply > 0 && (_board.isTwofoldRepetition() || _board.halfMoveClock >= 100)) {
        return 0; // Draw score but negative to prefer fighting for more over drawing moves
    }

    int ttScore;
    Representation::Moves::Move ttBestMove= Representation::Moves::DEFAULT_MOVE;

    bool isPVNode= (beta - alpha > 1);
    auto isTTHit= ProbeTT(depth, ply, alpha, beta, ttScore, ttBestMove);
    if(isTTHit && ply > 0) {
        return ttScore; // TT Hit with a cutoff
    }
    if(depth <= 0) return quiesce(alpha, beta, ply);

    auto isInCheck= _board.isInCheck();
    int staticEval= Evaluation::evaluate(_board);
    // Reverse Futility Pruning
    if(!isPVNode && !isInCheck && depth <= 5) {
        int rfpMargin= 120 * depth;
        if(staticEval - rfpMargin >= beta) {
            return staticEval; // Fail-hard cutoff
        }
    }

    // --- Null Move Pruning ---
    if(!isPVNode && tryNullMovePruning(depth, ply, alpha, beta)) {
        return beta;
    }

    MoveList moveList;
    MoveGeneration::Generator::GenerateMoves(_board, moveList);
    Search::MoveOrdering::SortMoves(_board, moveList, _tt, ply);

    int originalAlpha= alpha;
    int bestScore= -Evaluation::MATE_VAL;
    Representation::Moves::Move bestMoveThisNode;

    int legalMoveCount= 0; // Count of legal moves for statistics

    for(int i= 0; i < moveList.count; ++i) {

        auto& move= moveList.moves[i];
        _board.makeMove(move);

        bool isCapture= Representation::Moves::GetFlag(move) & Representation::Moves::CAPTURE_MASK;
        bool giveCheck= MoveGeneration::IsSquareAttacked(_board, (_board.activeColor == Pieces::WHITE) ? _board.whiteKingPos : _board.blackKingPos, (_board.activeColor == Pieces::WHITE) ? Pieces::BLACK : Pieces::WHITE);

        // Futility Pruning
        if(!isPVNode && !isInCheck && !giveCheck && depth <= 5 && legalMoveCount > 1) {
            int fpMargin= 100 * depth;

            if(staticEval + fpMargin <= alpha) {
                _board.undoMove();
                continue;
            }
        }

        // Deferred Legality Check:
        if(!MoveGeneration::IsLegalPosition(_board)) {
            _board.undoMove(); // Illegal move, undo it
            continue;          // Skip to next move
        }
        legalMoveCount++;

        // --- Late Move Pruning (LMP) ---
        bool isQuiet= !isCapture && !giveCheck;
        if(!isPVNode && !isInCheck && isQuiet && depth <= 8) {
            // Formula: Search more moves at higher depths before pruning
            int lmpThreshold= 3 + (depth * depth) / 2;
            if(legalMoveCount > lmpThreshold) {
                _board.undoMove();
                continue; // Prune this move and all subsequent quiet moves
            }
        }

        // --- Extensions ---
        int extension= GetExtension(_board, move, depth, giveCheck);

        // --- Late Move Reduction (LMR) ---
        int reduction= 0;
        if(legalMoveCount > 1) reduction= GetLateMoveReduction(depth, legalMoveCount - 1, isPVNode, isCapture, isInCheck);

        int evalDepth= depth - 1 + extension;
        if(evalDepth < 0) evalDepth= 0;
        int evaluation;

        if(legalMoveCount == 1) {
            evaluation= -search(evalDepth, ply + 1, -beta, -alpha); // No reduction for the first legal move
        } else {
            if(reduction > 0) {
                int reducedDepth= evalDepth - reduction;
                if(reducedDepth < 0) reducedDepth= 0;
                // Reduced Search: Use a "Zero-Window" to quickly prove the move is bad
                evaluation= -search(reducedDepth, ply + 1, -alpha - 1, -alpha);

                // Re-search: If the move unexpectedly beats alpha, it was actually good!
                // We must re-search it at full depth and with the full window.
                if(evaluation > alpha) {

                    evaluation= -search(evalDepth, ply + 1, -alpha - 1, -alpha);
                }
            } else {

                evaluation= -search(evalDepth, ply + 1, -alpha - 1, -alpha);
            }
            if(evaluation > alpha && evaluation < beta) {
                evaluation= -search(evalDepth, ply + 1, -beta, -alpha); // Full re-search with full window
            }
        }

        _board.undoMove();

        if(_searchAborted) return 0; // If we were signaled to stop during the search, return immediately

        if(evaluation > bestScore) {
            bestScore= evaluation;
            bestMoveThisNode= move;
        }
        if(evaluation >= beta) {
            bool isCapture= Representation::Moves::GetFlag(move) & Representation::Moves::CAPTURE_MASK;
            if(!isCapture) {
                Search::Killer::Store(ply, move);
                int from= Representation::Moves::GetFrom(move);
                int to= Representation::Moves::GetTo(move);
                Search::History::Store(_board.activeColor, from, to, depth);
            }
            break; // No need to search further moves at this node
        }
        if(evaluation > alpha) {
            alpha= evaluation;
        }
    }

    if(legalMoveCount == 0) {
        if(_board.isInCheck()) {
            _stats.checkMatesFound++;
            return -Evaluation::MATE(ply); // We are checkmated
        }
        return Evaluation::STALEMATE_SCORE; // Stalemate score (e.g., 0)
    }

    StoreTT(depth, ply, bestScore, originalAlpha, beta, bestMoveThisNode);
    return bestScore;
}
int Bot::quiesce(int alpha, int beta, int ply) {
    using namespace Representation;
    using namespace MoveGeneration;

    _stats.positionsEvaluated++;

    int ttScore;
    Representation::Moves::Move ttBestMove= Representation::Moves::DEFAULT_MOVE;
    if(ProbeTT(0, ply, alpha, beta, ttScore, ttBestMove)) {
        return ttScore;
    }
    int originalAlpha= alpha;

    bool isInCheck= _board.isInCheck();

    // Stand Pat
    if(!isInCheck) {
        int stand_pat= Evaluation::evaluate(_board);
        if(stand_pat >= beta) return beta; // Board is already too good enough, no need to search captures
        if(alpha < stand_pat) alpha= stand_pat;
    }
    //  Generate legal moves to check for captures/promotions
    MoveList moves;
    if(isInCheck) {
        Generator::GenerateMoves(_board, moves); // If in check, we must consider all moves to get out of check
    } else {
        Generator::GenerateCaptureMoves(_board, moves);
    }
    Search::MoveOrdering::SortMoves(_board, moves, _tt, ply);

    int legalMoves= 0; // Count of legal moves for checkmate detection in quiescence
    for(const auto& move: moves) {
        auto flag= Moves::GetFlag(move);

        // If the capture loses material, don't search it in QS
        if(!isInCheck && Evaluation::SEE(_board, move) < 0) {
            continue;
        }

        _board.makeMove(move);

        // Deferred Legality Check:
        if(!MoveGeneration::IsLegalPosition(_board)) {
            _board.undoMove();
            continue;
        }
        legalMoves++;

        int score= -quiesce(-beta, -alpha, ply + 1);
        _board.undoMove();

        if(score >= beta) {
            StoreTT(0, ply, score, originalAlpha, beta, move); // Store the cutoff move in TT
            return beta;                                       // Cutoff, no need to look at more captures
        }
        if(score > alpha) {
            alpha= score;
        }
    }
    if(isInCheck && legalMoves == 0) {
        // If we are in check and no captures improved our position, this is a checkmate
        return -Evaluation::MATE(ply);
    }
    StoreTT(0, ply, alpha, originalAlpha, beta, Representation::Moves::DEFAULT_MOVE); // Store the best move found in TT (or default if no captures)
    return alpha;
}
bool Bot::IsFinishThinking() {
    if(_currentSearchConfig.limit.depth > 0) {
        if(_currentSearchInfo.depthReached >= _currentSearchConfig.limit.depth) {
            std::cout << "info string Depth limit reached: " << _currentSearchInfo.depthReached << "\n";
            return true; // Stop if we've reached the depth limit (Hard Limit)
        }
        return false; // Don't check other limits if depth is set, we rely on it to stop the search
    }

    if(!_timeController.IsOverTime()) return false; // If we are not over time, keep searching

    // Assume we only went over the soft limit if we are here
    if(GetPVSearchStabilityProbability() < 0.75) {
        // We are pretty unstable (< 75% probability of stability) so we should shift the soft time limit up a bit to allow the search to stabilize and find a better move instead of stopping immediately on the edge of the time limit.

        // Move the softlimit up to half way between the current elapsed time and the hard time limit, giving the search a bit more time to stabilize and find a better move instead of stopping immediately on the edge of the time limit.
        // TODO: Experiment with more sophisticated methods of adjusting the soft limit based on the stability probability and how much time is left. For example, if we are very unstable (< 50% probability) we could be more aggressive in giving it extra time, while if we are somewhat stable (50-75%) we could give it a smaller boost.
        // _timeController.AdjustSoftLimit();
    }
    // We changed the softime, if we stil over time then we must have hit the hard time limit and should stop.
    return _timeController.IsOverTime(); // Stop if we are over the hard time limit
}

std::pair<Representation::Moves::Move, int> Bot::getBestMove(SearchConfig config) {

    // Calculate the time limits that we should actual use for this move based on the SearchConfig
    _currentSearchConfig= config;
    _timeController.Setup(_currentSearchConfig.limit, _board);

    Search::History::Clear();
    Search::Killer::Clear();
    _stats.reset();
    _searchAborted= false;

    int bestScore= 0;
    auto bestMoveOverall= Representation::Moves::DEFAULT_MOVE;

    // --- NEW: Panic Fallback Move ---
    // Initialize bestMoveOverall to the first legal move to prevent "a1a1" on immediate timeouts.
    MoveGeneration::MoveList rootMoves;
    MoveGeneration::Generator::GenerateMoves(_board, rootMoves);
    for(int i= 0; i < rootMoves.count; ++i) {
        _board.makeMove(rootMoves.moves[i]);
        if(MoveGeneration::IsLegalPosition(_board)) {
            bestMoveOverall= rootMoves.moves[i];
            _board.undoMove();
            break; // We found our emergency safety net move
        }
        _board.undoMove();
    }

    int depthReached= 0;

    // Aspiration Window
    int alpha= -Evaluation::INF;
    int beta= Evaluation::INF;
    int delta= 50; // Initial window size in centipawns
    int depth= 1;

    while(!IsFinishThinking()) { // Iterative deepening
        if(depth >= 4) {
            alpha= std::max(bestScore - delta, -Evaluation::INF);
            beta= std::min(bestScore + delta, Evaluation::INF);
        } else {
            alpha= -Evaluation::INF;
            beta= Evaluation::INF;
        }

        bool exactScoreFound= false;
        while(!exactScoreFound) {
            auto currentScore= search(depth, 0, alpha, beta);
            if(_searchAborted) break; // If we were signaled to stop during the search, break out of the aspiration loop immediately

            if(currentScore <= alpha) {
                // Fail-Low: Move is worse than expected, widen window downwards
                alpha= -Evaluation::INF;
            } else if(currentScore >= beta) {
                // Fail-High: Move is better than expected, widen window upwards
                beta= Evaluation::INF;
            } else {
                // Score is within the window, no need to re-search
                bestScore= currentScore;
                exactScoreFound= true;
            }
        }
        if(_searchAborted) break; // If we were signaled to stop during the search, break out of the loop immediately
        depthReached= depth;

        auto elapsedTime= _timeController.GetElapsedTime();
        // Output search info for GUI

        _currentSearchInfo.depthReached= depthReached;
        _currentSearchInfo.bestScore= bestScore;
        _currentSearchInfo.timeElapsed= elapsedTime;
        _currentSearchInfo.pvLine= extractPV();
        _currentSearchInfo.stats= _stats;
        _currentSearchInfo.stabilityProbability= GetPVSearchStabilityProbability();

        printSearchInfo(_currentSearchInfo);

        // If the TT chain broke at the root and returned empty, force it to display the known best move.
        if(_currentSearchInfo.pvLine.empty() && bestMoveOverall != Representation::Moves::DEFAULT_MOVE) {
            _currentSearchInfo.pvLine= Representation::Moves::toString(bestMoveOverall);
        }

        // Retrieve the best move from the Transposition Table for the root position
        auto& ttEntry= _tt.probe(_board.zobristHash);

        if(ttEntry.zobristHash == _board.zobristHash && ttEntry.bestMove != Representation::Moves::DEFAULT_MOVE) {
            bestMoveOverall= ttEntry.bestMove;
        } else {
            // If the TT somehow didn't have a move (e.g., checkmate/stalemate with no legal moves)
            if(bestMoveOverall == Representation::Moves::DEFAULT_MOVE) {
                std::cerr << "No legal moves found at depth " << depth << ", returning default move." << std::endl;
            }
        }
        depth++;
    }

    std::cout << "info string Search finished. Depth reached: " << depthReached << ", Best Score: " << bestScore << ", Best Move: " << Representation::Moves::toString(bestMoveOverall) << std::endl;
    return {bestMoveOverall, bestScore};
}
void Bot::printSearchInfo(SearchInfo searchInfo) {
    long long nodesPerMs= (searchInfo.stats.positionsEvaluated) / (searchInfo.timeElapsed + 1);
    long long nps= nodesPerMs * 1000UL;
    std::string scoreStr;
    // UCI scores are always from side-to-move's perspective
    if(searchInfo.bestScore > Evaluation::MATE_VAL - 100) {
        // Side to move is delivering mate
        int pliesToMate= Evaluation::MATE_VAL - searchInfo.bestScore;
        int movesToMate= (pliesToMate + 1) / 2;
        scoreStr= "mate " + std::to_string(movesToMate);
    } else if(searchInfo.bestScore < -Evaluation::MATE_VAL + 100) {
        // Side to move is getting mated
        int pliesToMate= searchInfo.bestScore + Evaluation::MATE_VAL;
        int movesToMate= (pliesToMate + 1) / 2;
        scoreStr= "mate -" + std::to_string(movesToMate);
    } else {
        scoreStr= "cp " + std::to_string(searchInfo.bestScore);
    }
    // UCI specific
    std::cout << "info" << " depth " << searchInfo.depthReached
              << " score " << scoreStr
              << " time " << searchInfo.timeElapsed
              << " nodes " << searchInfo.stats.positionsEvaluated
              << " nps " << nps
              << " hashfull " << (searchInfo.stats.ttStores > 0 ? (searchInfo.stats.ttStores * 1000 / _tt.size()) : 0)
              << " stability " << static_cast<int>(searchInfo.stabilityProbability * 100) << "%"
              << " pv" << searchInfo.pvLine << std::endl;
}
std::string Bot::extractPV() {
    std::string pvStr;
    auto tempBoard= _board; // Create a temporary copy of the board to traverse the PV
    int count= 0;
    int maxPVLength= 64; // Limit PV length to prevent infinite loops in case of bugs
    while(count < maxPVLength) {
        auto& ttEntry= _tt.probe(tempBoard.zobristHash);
        if(ttEntry.zobristHash != tempBoard.zobristHash || ttEntry.bestMove == Representation::Moves::DEFAULT_MOVE) {
            break; // No valid TT entry or no move stored, end of PV
        }

        auto pvMove= ttEntry.bestMove;

        MoveGeneration::MoveList moves;
        MoveGeneration::Generator::GenerateMoves(tempBoard, moves);
        bool isValidMove= false;

        for(int i= 0; i < moves.count; ++i) {
            if(moves.moves[i] == pvMove) {
                isValidMove= true;
                break;
            }
        }

        if(!isValidMove) break; // Hash collision, stop extracting

        pvStr+= " " + Representation::Moves::toString(pvMove);
        tempBoard.makeMove(pvMove);
        count++;
        if(tempBoard.isTwofoldRepetition()) {
            break; // Stop PV extraction if we hit a repetition since its a draw and there is no point in following the PV further as it will likely be inaccurate and just lead us in circles
        }
    }
    return pvStr;
}
} // namespace Knilb::Engine
