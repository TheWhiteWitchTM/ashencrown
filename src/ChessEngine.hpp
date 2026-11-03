#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct Move {
    int from = 0;
    int to = 0;
    int promo = 0; // 0, or 2..5 (knight, bishop, rook, queen)
    int kind = 0;  // 0 quiet or capture, 1 castle, 2 en passant
    bool operator==(const Move& o) const {
        return from == o.from && to == o.to && promo == o.promo && kind == o.kind;
    }
};

class ChessEngine {
public:
    ChessEngine();

    void reset();
    bool setFen(const std::string& fen);
    std::string fen() const;

    int pieceAt(int sq) const { return board_[sq]; }
    int sideToMove() const { return side_; }
    int epSquare() const { return ep_; }
    int castleRights() const { return castle_; }
    int halfmoveClock() const { return half_; }
    uint64_t key() const { return key_; }

    bool inCheck(int color) const;
    bool insufficient() const;

    std::vector<Move> legalMoves();
    // Legal moves as if `side` were to move. The board, side, and key are left as they were.
    std::vector<Move> legalMovesFor(int side);
    // Squares of `byColor` pieces that attack sq. Pins are ignored.
    std::vector<int> attackers(int sq, int byColor) const;
    // After `bySide` plays `cap`, the square of a piece that can take on the landing square, or -1.
    int recaptureFrom(const Move& cap, int bySide);
    // Legal landing squares for the piece on sq, as if it were that side's turn.
    std::vector<int> targetsFrom(int sq);
    std::string san(const Move& m);
    bool pick(int from, int to, int promo, Move& out);

    void make(const Move& m);
    void unmake();

    uint64_t perft(int depth);

    static int chessSelfTest();

private:
    struct Undo {
        Move move;
        int moved = 0;
        int captured = 0;
        int capSq = -1;
        int castle = 0;
        int ep = -1;
        int half = 0;
        int full = 1;
        int side = 0;
    };

    std::array<int, 64> board_{};
    int side_ = 0;
    int castle_ = 0;
    int ep_ = -1;
    int half_ = 0;
    int full_ = 1;
    uint64_t key_ = 0;
    std::vector<Undo> history_;

    void generate(std::vector<Move>& out) const;
    bool attacked(int sq, int byColor) const;
    void recomputeKey();
    void ensureZobrist();

    static uint64_t Z_[2][7][64];
    static uint64_t ZSide_;
    static uint64_t ZCastle_[16];
    static uint64_t ZEp_[8];
    static bool zReady_;
};
