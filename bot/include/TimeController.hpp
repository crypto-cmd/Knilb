#pragma once

#include "Piece.hpp"
#include "Search.hpp"
#include "ThinkStopwatch.hpp"
#include <chrono>
#include <cmath>

namespace Knilb::Engine {

class TimeController {
  public:
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

  private:
    std::chrono::steady_clock::time_point _startTime;
    double _softTimeLimit= 0; // Time limit to stop "intelligently" based on PV stability and other heuristics
    double _hardTimeLimit= 0; // Absolute time limit to stop the search regardless of heuristics
    bool _isTimeLimitSet= false;
    bool _stopSearch= false;

  public:
    TimeController()= default;
    void Setup(const SearchLimit& limit, const Representation::Board& board) {
        _isTimeLimitSet= true;
        _stopSearch= false;
        _startTime= std::chrono::steady_clock::now();

        // 1. Pondering or Infinite: NO LIMITS
        if(limit.ponder || limit.infinite) {
            _softTimeLimit= 1e18; // Effectively infinite
            _hardTimeLimit= 1e18;
            return;
        }

        // 2. Fixed Depth or Nodes: NO TIME LIMITS (Logical limits handle the stop)
        if(limit.depth > 0) {
            _softTimeLimit= 1e18;
            _hardTimeLimit= 1e18;
            return;
        }

        // 3. Explicit Move Time: HARD LIMIT ONLY
        if(limit.movetime > 0) {
            // We set Soft = Hard so we don't stop early "intelligently"
            // We want to use the full time allotted.
            _softTimeLimit= 1e18; // Don't stop early, use the full time allotted
            _hardTimeLimit= limit.movetime;
            return;
        }

        // 4. Standard Time Control (wtime, btime, winc, binc)
        int myTime= (board.activeColor == Representation::Pieces::WHITE) ? limit.wtime : limit.btime;
        int myInc= (board.activeColor == Representation::Pieces::WHITE) ? limit.winc : limit.binc;

        double movesRemaining= 40.0; // Default flat divisor for sudden death

        if(limit.movestogo > 0) {
            movesRemaining= limit.movestogo;
        } else {
            // Dynamic divisor based on remaining time to prevent flagging in fast games
            if(myTime < 30000) movesRemaining= 50.0;
            if(myTime < 10000) movesRemaining= 60.0;
        }

        // Safety buffer (lag compensation)
        int overhead= 200;

        // Calculate Soft Limit
        double timeSlot= (double(myTime) / movesRemaining) + myInc * 0.75 - overhead;

        // Ensure we don't get a negative time if we are very low on time
        _softTimeLimit= std::max(50.0, timeSlot);

        // Hard limit capped at 40% of total remaining time to absolutely prevent flagging
        _hardTimeLimit= std::min(double(myTime) * 0.4, _softTimeLimit * 5.0);
    }
    double GetElapsedTime() const {
        auto now= std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - _startTime).count();
    }
    bool IsOverTime() {
        if(!_isTimeLimitSet) return false;
        auto elapsed= GetElapsedTime();

        // Must stop if we hit the hard limit
        if(elapsed >= _hardTimeLimit) {
            _stopSearch= true;
            return true;
        }

        if(elapsed >= _softTimeLimit) {
            // We could implement additional heuristics here to decide whether to stop immediately or keep going for a bit longer if the PV is unstable, but for now we'll just stop immediately when we hit the soft limit.
            _stopSearch= true;
            return true;
        }
        return false;
    }
};
}; // namespace Knilb::Engine