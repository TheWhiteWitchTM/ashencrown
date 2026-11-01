#include "ChessAI.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>

namespace {

int PST[7][64];
bool pstReady = false;

void revInto(int* dst, const int* srcRank8) {
    for (int r = 0; r < 8; ++r)
        for (int f = 0; f < 8; ++f) dst[r * 8 + f] = srcRank8[(7 - r) * 8 + f];
}

void initPst() {
    if (pstReady) return;
    const int pawn[] = {
        0,  0,  0,  0,  0,  0,  0,  0,  50, 50, 50, 50, 50, 50, 50, 50, 10, 10, 20, 30, 30, 20, 10, 10, 5,  5,  10, 25,
        25, 10, 5,  5,  0,  0,  0,  20, 20, 0,  0,  0,  5,  -5, -10, 0,  0,  -10, -5, 5,  5,  10, 10, -20, -20, 10, 10, 5,
        0,  0,  0,  0,  0,  0,  0,  0};
    const int knight[] = {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,   0,   -20, -40, -30, 0,   10,  15, 15,
                          10,  0,   -30, -30, 5,   15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,   -30, -30, 5,
                          10,  15,  15,  10,  5,   -30, -40, -20, 0,   5,   5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};
    const int bishop[] = {-20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,   0,   0,   -10, -10, 0,   5,   10,  10,
                          5,   0,   -10, -10, 5,   5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,   -10, -10, 10,
                          10,  10,  10,  10,  10,  -10, -10, 5,   0,   0,   0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};
    const int rook[] = {0,  0, 0,  0,  0,  0,  0,  0,  5,  10, 10, 10, 10, 10, 10, 5,  -5, 0,  0,  0, 0,  0,
                        0,  -5, -5, 0,  0,  0,  0,  0,  0,  -5, -5, 0,  0,  0,  0,  0,  0,  -5, -5, 0, 0, 0, 0, 0,
                        0,  -5, -5, 0,  0,  0,  0,  0,  0,  -5, 0,  0,  0,  5,  5,  0,  0,  0};
    const int queen[] = {-20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   0,   0,  0,  0,   0,   -10, -10, 0,   5,   5,   5,   5,
                         0,   -10, -5,  0,   5,  5,   5,  5,  0,  -5, -5, 0,   5,  5,  5,  5,  0,   -5, -10, 0,   5,   5,   5,   5,
                         0,   -10, -10, 0,   0,  0,   0,  0,   0,  -10, -20, -10, -10, -5, -5, -10, -10, -20};
    const int king[] = {-30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40,
                        -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20,
                        -20, -20, -20, -10, 20,  20,  0,   0,   0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20};
    revInto(PST[1], pawn);
    revInto(PST[2], knight);
    revInto(PST[3], bishop);
    revInto(PST[4], rook);
    revInto(PST[5], queen);
    revInto(PST[6], king);
    pstReady = true;
}

const int kVal[7] = {0, 100, 320, 330, 500, 900, 0};

}  // namespace

Move ChessAI::think(ChessEngine& engine, int maxDepth, int timeMs, std::atomic<bool>* stop) {
    initPst();
    tt_.assign(1 << 18, {});
    for (auto& row : killers_) row[0] = row[1] = {};
    stop_ = stop;
    aborted_ = false;
    nodes_ = 0;
    deadline_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count() +
                std::max(40, timeMs);
    auto moves = engine.legalMoves();
    if (moves.empty()) return {};
    Move committed = moves.front();
    rootBest_ = committed;
    for (int d = 1; d <= maxDepth; ++d) {
        aborted_ = false;
        rootBest_ = committed;
        search(engine, d, -kInf, kInf, 0);
        if (!aborted_ || d == 1) committed = rootBest_;
        if (aborted_) break;
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now().time_since_epoch())
                             .count();
        if (now > deadline_) break;
    }
    return committed;
}

bool ChessAI::timesUp() {
    if (aborted_) return true;
    if (stop_ && stop_->load(std::memory_order_relaxed)) return true;
    if ((nodes_ & 2047) != 0) return false;
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now().time_since_epoch())
                         .count();
    return now > deadline_;
}

int ChessAI::evaluate(const ChessEngine& eng) const {
    int score = 0;
    int bishops[2] = {0, 0};
    for (int s = 0; s < 64; ++s) {
        int p = eng.pieceAt(s);
        if (!p) continue;
        int color = p > 0 ? 0 : 1;
        int t = std::abs(p);
        int idx = color == 0 ? s : (s ^ 56);
        int v = kVal[t] + PST[t][idx];
        score += color == 0 ? v : -v;
        if (t == 3) ++bishops[color];
    }
    if (bishops[0] >= 2) score += 30;
    if (bishops[1] >= 2) score -= 30;
    return eng.sideToMove() == 0 ? score : -score;
}

void ChessAI::order(ChessEngine& eng, std::vector<Move>& moves, const Move& hashMove, int ply) {
    auto rank = [&](const Move& m) {
        if (m == hashMove) return 100000;
        int cap = 0;
        if (m.kind == 2) cap = 1;
        else cap = std::abs(eng.pieceAt(m.to));
        if (cap) return 10000 + cap * 10 - std::abs(eng.pieceAt(m.from));
        if (ply < 64) {
            if (m == killers_[ply][0]) return 4000;
            if (m == killers_[ply][1]) return 3900;
        }
        int t = std::abs(eng.pieceAt(m.from));
        int idx = eng.sideToMove() == 0 ? m.to : (m.to ^ 56);
        return PST[t][idx];
    };
    std::stable_sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) { return rank(a) > rank(b); });
}

int ChessAI::qsearch(ChessEngine& eng, int alpha, int beta, int ply) {
    ++nodes_;
    if (timesUp()) {
        aborted_ = true;
        return 0;
    }
    const bool check = eng.inCheck(eng.sideToMove());
    if (!check) {
        int stand = evaluate(eng);
        if (stand >= beta) return beta;
        if (stand > alpha) alpha = stand;
    }
    if (ply > 10) return evaluate(eng);
    auto moves = eng.legalMoves();
    for (const Move& m : moves) {
        bool cap = m.kind == 2 || eng.pieceAt(m.to) != 0;
        if (!check && !cap) continue;
        eng.make(m);
        int score = -qsearch(eng, -beta, -alpha, ply + 1);
        eng.unmake();
        if (aborted_) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

int ChessAI::search(ChessEngine& eng, int depth, int alpha, int beta, int ply) {
    ++nodes_;
    if (timesUp()) {
        aborted_ = true;
        return 0;
    }
    const int origAlpha = alpha;
    Move hashMove;
    TTEntry* slot = nullptr;
    if (!tt_.empty()) {
        slot = &tt_[eng.key() & (tt_.size() - 1)];
        if (slot->key == eng.key() && slot->depth >= depth && std::abs(slot->score) < 20000) {
            if (slot->flag == 0) return slot->score;
            if (slot->flag == 1 && slot->score <= alpha) return alpha;
            if (slot->flag == 2 && slot->score >= beta) return beta;
        }
        if (slot->key == eng.key()) hashMove = slot->move;
    }

    if (depth <= 0) return qsearch(eng, alpha, beta, ply);

    auto moves = eng.legalMoves();
    if (moves.empty()) return eng.inCheck(eng.sideToMove()) ? -30000 + ply : 0;
    order(eng, moves, hashMove, ply);

    Move bestMove = moves.front();
    int best = -kInf;
    for (const Move& m : moves) {
        eng.make(m);
        int score = -search(eng, depth - 1, -beta, -alpha, ply + 1);
        eng.unmake();
        if (aborted_) return 0;
        if (score > best) {
            best = score;
            bestMove = m;
            if (ply == 0) rootBest_ = m;
        }
        if (score > alpha) alpha = score;
        if (score >= beta) {
            if (ply < 64 && eng.pieceAt(m.to) == 0 && m.kind != 2) {
                killers_[ply][1] = killers_[ply][0];
                killers_[ply][0] = m;
            }
            break;
        }
    }

    if (slot && !aborted_) {
        slot->key = eng.key();
        slot->depth = depth;
        slot->score = best;
        slot->move = bestMove;
        if (best <= origAlpha) slot->flag = 1;
        else if (best >= beta) slot->flag = 2;
        else slot->flag = 0;
    }
    return best;
}
