#pragma once

#include "Board.hpp"
#include "Coordinate.hpp"
#include "Evaluation.hpp"
#include "Move.hpp"
#include "MoveGeneration.hpp"
#include "Piece.hpp"
#include "TimeController.hpp"
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
    float stabilityProbability;
};


struct SearchConfig {
    TimeController::SearchLimit limit;

    // Engine parameters
    int hashMB= 0;
    int contempt= 0;

    // Restrict search to these moves (UCI "searchmoves") std::vector<Move> searchMoves; // or
    std::vector<std::string> moves;

    // Debug / dev
    bool verbose= false;
    bool trace= false;
};

class Bot {

    std::atomic<bool>* _stopFlag= nullptr;

    static constexpr int MAX_PLY= 64;
    static constexpr int NUM_KILLERS= 2;
    Representation::Moves::Move _killerMoves[MAX_PLY][NUM_KILLERS]= {};

    bool tryNullMovePruning(int depth, int ply, int alpha, int beta);
    int GetLateMoveReduction(int depth, int moveIndex, bool isPVNode, bool isCapture, bool isCheck);
    int GetExtension(const Representation::Board& board, const Representation::Moves::Move& move, int depth, bool isInCheck);

    private:
    Representation::Board _board;
    Engine::Search::TranspositionTable _tt;
    bool _searchAborted= false;
    SearchStatistics _stats;

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
    TimeController _timeController;

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
};

} // namespace Knilb::Engine