#pragma once

#include "ChessEngine.hpp"

#include <atomic>
#include <cstdint>
#include <vector>

class ChessAI {
public:
    Move think(ChessEngine& engine, int maxDepth, int timeMs, std::atomic<bool>* stop);

private:
    static constexpr int kInf = 100000;
    struct TTEntry {
        uint64_t key = 0;
        int depth = -1;
        int score = 0;
        int flag = 0; // 0 exact, 1 upper, 2 lower
        Move move;
    };

    std::vector<TTEntry> tt_;
    Move killers_[64][2];
    std::atomic<bool>* stop_ = nullptr;
    int nodes_ = 0;
    std::int64_t deadline_ = 0;
    bool aborted_ = false;
    Move rootBest_;

    int search(ChessEngine& eng, int depth, int alpha, int beta, int ply);
    int qsearch(ChessEngine& eng, int alpha, int beta, int ply);
    int evaluate(const ChessEngine& eng) const;
    bool timesUp();
    void order(ChessEngine& eng, std::vector<Move>& moves, const Move& hashMove, int ply);
};
