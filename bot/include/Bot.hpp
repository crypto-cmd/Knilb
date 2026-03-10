#pragma once

#include "Board.hpp"
#include "Coordinate.hpp"
#include "Evaluation.hpp"
#include "Move.hpp"
#include "MoveGeneration.hpp"
#include "Piece.hpp"
#include "ThinkStopwatch.hpp"
#include "TranspositionTable.hpp"
#include <atomic>
#include <chrono>
#include <functional>
#include <random>
#include <string>
#include <utility>

namespace Knilb::Engine {
using namespace Representation;

struct SearchStatistics {
    long long positionsEvaluated= 0;
    long long checkMatesFound= 0;

    // TT Statistics
    long long ttProbes= 0;
    long long ttHits= 0;
    long long ttCutoffs= 0;
    long long ttStores= 0;
    long long ttOverwrites= 0;

    void reset() {
        positionsEvaluated= 0;
        checkMatesFound= 0;
        ttProbes= 0;
        ttHits= 0;
        ttCutoffs= 0;
        ttStores= 0;
        ttOverwrites= 0;
    }
};

struct SearchInfo {
    int depthReached;
    int bestScore;
    int timeElapsed;
    std::string pvLine;
    SearchStatistics stats;
    float stabilityProbability; // New field to indicate how stable the PV move is across iterations
};

struct SearchLimit {
    // Time Control
    int wtime= 0;     // Time left for white in ms
    int btime= 0;     // Time left for black in ms
    int winc= 0;      // Increment per move for white in ms
    int binc= 0;      // Increment per move for black in ms
    int movestogo= 0; // Moves until next time control (if known)
    // Hard Limits
    int depth= 0;    // Search depth limit
    int movetime= 0; // Time limit for this move in ms

    // Mode flags
    bool ponder= false;
    bool infinite= false;
};
struct SearchConfig {
    SearchLimit limit;

    // Engine parameters
    int hashMB= 0;
    int contempt= 0; // centipawns

    // Restrict search to these moves (UCI "searchmoves") std::vector<Move> searchMoves; // or
    std::vector<std::string> moves;

    // Debug / dev
    bool verbose= false;
    bool trace= false;
};

class Bot {
    // Killer Moves: 2 killers per ply, max 64 plies

    std::atomic<bool>* _stopFlag= nullptr;

    static constexpr int MAX_PLY= 64;
    static constexpr int NUM_KILLERS= 2;
    Representation::Moves::Move _killerMoves[MAX_PLY][NUM_KILLERS]= {};

    // --- Search Enhancements ---
    // Null Move Pruning
    bool tryNullMovePruning(int depth, int ply, int alpha, int beta);
    // Late Move Reduction
    int GetLateMoveReduction(int depth, int moveIndex, bool isPVNode, bool isCapture, bool isCheck);
    // Extensions
    int GetExtension(const Representation::Board& board, const Representation::Moves::Move& move, int depth, bool isInCheck);

    private:
    Representation::Board _board;
    Engine::Search::TranspositionTable _tt;
    bool _searchAborted= false;
    SearchStatistics _stats;

    // Time Management
    ThinkStopwatch _stopwatch;

  public:
    Bot();
    // Inject the stop flag from the main thread
    void setStopFlag(std::atomic<bool>* flag) {
        _stopFlag= flag;
    }

    void setFen(const std::string& fen);
    void PerformMove(const std::string& moveStr);
    std::pair<Representation::Moves::Move, int> getBestMove(SearchConfig);

    const Representation::Board& getBoard() const {
        return _board;
    }

    inline void resizeHash(size_t sizeInMB) {
        _tt.resize(sizeInMB);
    }

  private:
    SearchConfig _currentSearchConfig;
    SearchInfo _currentSearchInfo;
    bool _isTimeLimitSet= false;
    int _hardTimeLimit= 0;
    int _softTimeLimit= 0;

    int quiesce(int alpha, int beta, int ply);
    int search(int depth, int ply, int alpha, int beta);
    std::string extractPV();
    void printSearchInfo(SearchInfo searchInfo);
    void StoreTT(int depth, int ply, int bestScore, int originalAlpha, int beta, Representation::Moves::Move bestMove);
    bool ProbeTT(int depth, int ply, int alpha, int beta, int& ttScore, Representation::Moves::Move& ttBestMove);
    bool IsFinishThinking();

    float GetPVSearchStabilityProbability() {
        return 1; // Assume 100% stability for now

        // TODO: Implement a heuristic to estimate the stability of the PV move based on how much it has changed across iterations, TT hits, and other factors. This can help us make smarter decisions about when to stop the search on time limits.
    }

    void SetupTimeLimits() {
        _isTimeLimitSet= true;

        // 1. Pondering or Infinite: NO LIMITS
        if(_currentSearchConfig.limit.infinite) {
            _softTimeLimit= 1e18; // Effectively infinite
            _hardTimeLimit= 1e18;
            return;
        }

        // 2. Fixed Depth or Nodes: NO TIME LIMITS (Logical limits handle the stop)
        if(_currentSearchConfig.limit.depth > 0) {
            _softTimeLimit= 1e18;
            _hardTimeLimit= 1e18;
            return;
        }

        // 3. Explicit Move Time: HARD LIMIT ONLY
        if(_currentSearchConfig.limit.movetime > 0) {
            // We set Soft = Hard so we don't stop early "intelligently"
            // We want to use the full time allotted.
            _softTimeLimit= 1e18; // Don't stop early, use the full time allotted
            _hardTimeLimit= 1e18; // Rely on the move time limit to stop the search
            std::cout << "info string Using fixed move time limit: " << _currentSearchConfig.limit.movetime << " ms\n";
            return;
        }

        // 4. Standard Time Control (wtime, btime, winc, binc)
        // ONLY HERE do we use the "Soft Limit" logic
        int theoreticalMovesPerTimeControl= (30 - (_board.fullMoveNumber / 2));
        if(theoreticalMovesPerTimeControl < 10) theoreticalMovesPerTimeControl= 10;                                                           // Don't allow it to get too low, we don't want the time management to get too aggressive in the late endgame just because we are low on time and moves remaining is low
        int movesRemaining= _currentSearchConfig.limit.movestogo > 0 ? _currentSearchConfig.limit.movestogo : theoreticalMovesPerTimeControl;
        int myTime= (_board.activeColor == Pieces::WHITE) ? _currentSearchConfig.limit.wtime : _currentSearchConfig.limit.btime;
        int myInc= (_board.activeColor == Pieces::WHITE) ? _currentSearchConfig.limit.winc : _currentSearchConfig.limit.binc;

        // Safety buffer (lag compensation)
        int overhead= 200;

        // Calculate Soft Limit
        double timeSlot= (double(myTime) / movesRemaining) + myInc * 0.75 - overhead;

        // Ensure we don't get a negative time if we are very low on time
        _softTimeLimit= std::max(50.0, timeSlot);

        // Hard Limit is usually 5x soft, but never more than 50% of remaining time
        _hardTimeLimit= std::min(double(myTime) * 0.5, _softTimeLimit * 5.0);
        std::cout << "info string Time management: myTime=" << myTime << " ms, myInc=" << myInc << " ms, movesRemaining=" << movesRemaining << "\n";
        std::cout << "info string Calculated time limits: softTimeLimit=" << _softTimeLimit << " ms, hardTimeLimit=" << _hardTimeLimit << " ms\n";
    }
};

} // namespace Knilb::Engine