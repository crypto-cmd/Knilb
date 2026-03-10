#pragma once
#include "Board.hpp"
#include "Evaluation.hpp"
#include "Move.hpp"
#include "Piece.hpp"
#include "Zobrist.hpp"
#include <span>
#include <vector>
namespace Knilb::Engine::Search {
// Transposition Table Flags
enum TTFlag : uint8_t {
    TT_EXACT, // Exact score (score is exact)
    TT_ALPHA, // Upper Bound (>= score)
    TT_BETA   // Lower Bound (<= score)
};

struct TTEntry {
    Precomputed::Zobrist::ZobristHash zobristHash;
    Representation::Moves::Move bestMove;
    int score;
    int depth;
    TTFlag flag;
};

class TranspositionTable {
  private:
    std::vector<TTEntry> _table;

  public:
    TranspositionTable(size_t sizeInMB= 16) {
        resize(sizeInMB);
    }
    void clear() {
        for(auto& entry: _table) {
            entry.bestMove= Representation::Moves::DEFAULT_MOVE; // Initialize to default move
            entry.zobristHash= 0;
            entry.depth= -1;
            entry.flag= TT_EXACT;
            entry.score= -Evaluation::INF;
        }
    }
    void resize(size_t sizeInMB) {
        size_t targetEntries= (sizeInMB * 1024 * 1024) / sizeof(TTEntry);

        size_t powerOfTwoEntries= 1;
        while(powerOfTwoEntries <= targetEntries) {
            powerOfTwoEntries*= 2;
        }
        size_t numEntries= powerOfTwoEntries;
        _table.clear();
        _table.resize(numEntries);
        clear();
    }
    TTEntry& probe(Precomputed::Zobrist::ZobristHash zobristHash) {
        size_t index= zobristHash & (_table.size() - 1);
        auto& entry= _table[index];
        return entry;
    }
    void store(const TTEntry& entry) {
        size_t index= entry.zobristHash & (_table.size() - 1);

        auto& existingEntry= _table[index];
        if(existingEntry.zobristHash != entry.zobristHash || entry.depth >= existingEntry.depth) _table[index]= entry;
    }
    size_t size() const { return _table.size(); }
};
} // namespace Knilb::Engine::Search