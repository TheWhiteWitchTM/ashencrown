#include "ChessEngine.hpp"

#include <cctype>
#include <iostream>
#include <sstream>

namespace {

int fileOf(int s) { return s & 7; }
int rankOf(int s) { return s >> 3; }
int makeSq(int f, int r) { return (r << 3) | f; }
bool onBoard(int f, int r) { return (f & ~7) == 0 && (r & ~7) == 0; }

constexpr int kWK = 1;
constexpr int kWQ = 2;
constexpr int kBK = 4;
constexpr int kBQ = 8;

}  // namespace

uint64_t ChessEngine::Z_[2][7][64];
uint64_t ChessEngine::ZSide_;
uint64_t ChessEngine::ZCastle_[16];
uint64_t ChessEngine::ZEp_[8];
bool ChessEngine::zReady_ = false;

ChessEngine::ChessEngine() {
    ensureZobrist();
    reset();
}

void ChessEngine::ensureZobrist() {
    if (zReady_) return;
    uint64_t x = 0xA5C0FFEEULL;
    auto next = [&x]() {
        uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    };
    for (int c = 0; c < 2; ++c)
        for (int t = 1; t <= 6; ++t)
            for (int s = 0; s < 64; ++s) Z_[c][t][s] = next();
    ZSide_ = next();
    for (int i = 0; i < 16; ++i) ZCastle_[i] = next();
    for (int i = 0; i < 8; ++i) ZEp_[i] = next();
    zReady_ = true;
}

void ChessEngine::reset() {
    setFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void ChessEngine::recomputeKey() {
    uint64_t k = 0;
    for (int s = 0; s < 64; ++s) {
        int p = board_[s];
        if (!p) continue;
        k ^= Z_[p > 0 ? 0 : 1][std::abs(p)][s];
    }
    if (side_) k ^= ZSide_;
    k ^= ZCastle_[castle_ & 15];
    if (ep_ >= 0) k ^= ZEp_[fileOf(ep_)];
    key_ = k;
}

bool ChessEngine::setFen(const std::string& fen) {
    board_.fill(0);
    std::istringstream in(fen);
    std::string board, side, castle, ep;
    int half = 0, full = 1;
    if (!(in >> board >> side >> castle >> ep)) return false;
    in >> half >> full;
    int rank = 7, file = 0;
    for (char c : board) {
        if (c == '/') {
            if (file != 8) return false;
            --rank;
            file = 0;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            file += c - '0';
            continue;
        }
        int type = 0;
        switch (std::tolower(static_cast<unsigned char>(c))) {
        case 'p': type = 1; break;
        case 'n': type = 2; break;
        case 'b': type = 3; break;
        case 'r': type = 4; break;
        case 'q': type = 5; break;
        case 'k': type = 6; break;
        default: return false;
        }
        if (rank < 0 || file > 7) return false;
        int sign = std::isupper(static_cast<unsigned char>(c)) ? 1 : -1;
        board_[makeSq(file, rank)] = sign * type;
        ++file;
    }
    side_ = (side == "b") ? 1 : 0;
    castle_ = 0;
    if (castle.find('K') != std::string::npos) castle_ |= kWK;
    if (castle.find('Q') != std::string::npos) castle_ |= kWQ;
    if (castle.find('k') != std::string::npos) castle_ |= kBK;
    if (castle.find('q') != std::string::npos) castle_ |= kBQ;
    ep_ = -1;
    if (ep.size() == 2 && ep[0] >= 'a' && ep[0] <= 'h' && ep[1] >= '1' && ep[1] <= '8')
        ep_ = makeSq(ep[0] - 'a', ep[1] - '1');
    half_ = half;
    full_ = full > 0 ? full : 1;
    history_.clear();
    recomputeKey();
    return true;
}

std::string ChessEngine::fen() const {
    std::string out;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            int p = board_[makeSq(file, rank)];
            if (!p) {
                ++empty;
                continue;
            }
            if (empty) {
                out += char('0' + empty);
                empty = 0;
            }
            const char* letters = " PNBRQK";
            char c = letters[std::abs(p)];
            out += p > 0 ? c : char(std::tolower(static_cast<unsigned char>(c)));
        }
        if (empty) out += char('0' + empty);
        if (rank) out += '/';
    }
    out += side_ ? " b " : " w ";
    std::string castle;
    if (castle_ & kWK) castle += 'K';
    if (castle_ & kWQ) castle += 'Q';
    if (castle_ & kBK) castle += 'k';
    if (castle_ & kBQ) castle += 'q';
    out += castle.empty() ? "-" : castle;
    if (ep_ < 0) out += " -";
    else {
        out += ' ';
        out += char('a' + fileOf(ep_));
        out += char('1' + rankOf(ep_));
    }
    out += " " + std::to_string(half_) + " " + std::to_string(full_);
    return out;
}

bool ChessEngine::attacked(int sq, int byColor) const {
    const int ff = fileOf(sq);
    const int fr = rankOf(sq);
    const int pawnDir = byColor == 0 ? -1 : 1;
    const int pawn = byColor == 0 ? 1 : -1;
    for (int df : {-1, 1}) {
        int f = ff + df;
        int r = fr + pawnDir;
        if (onBoard(f, r) && board_[makeSq(f, r)] == pawn) return true;
    }
    const int knight = byColor == 0 ? 2 : -2;
    const int nd[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    for (auto d : nd) {
        int f = ff + d[0], r = fr + d[1];
        if (onBoard(f, r) && board_[makeSq(f, r)] == knight) return true;
    }
    const int king = byColor == 0 ? 6 : -6;
    for (int dr = -1; dr <= 1; ++dr)
        for (int df = -1; df <= 1; ++df) {
            if (!dr && !df) continue;
            int f = ff + df, r = fr + dr;
            if (onBoard(f, r) && board_[makeSq(f, r)] == king) return true;
        }
    const int bishop = byColor == 0 ? 3 : -3;
    const int rook = byColor == 0 ? 4 : -4;
    const int queen = byColor == 0 ? 5 : -5;
    const int dirs[8][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < 8; ++i) {
        int f = ff + dirs[i][0], r = fr + dirs[i][1];
        bool diagonal = i < 4;
        while (onBoard(f, r)) {
            int p = board_[makeSq(f, r)];
            if (p) {
                if (p == queen || p == (diagonal ? bishop : rook)) return true;
                break;
            }
            f += dirs[i][0];
            r += dirs[i][1];
        }
    }
    return false;
}

std::vector<int> ChessEngine::attackers(int sq, int byColor) const {
    std::vector<int> out;
    if (sq < 0 || sq > 63 || byColor < 0 || byColor > 1) return out;
    const int ff = fileOf(sq);
    const int fr = rankOf(sq);
    const int pawnDir = byColor == 0 ? -1 : 1;
    const int pawn = byColor == 0 ? 1 : -1;
    for (int df : {-1, 1}) {
        int f = ff + df;
        int r = fr + pawnDir;
        if (onBoard(f, r) && board_[makeSq(f, r)] == pawn) out.push_back(makeSq(f, r));
    }
    const int knight = byColor == 0 ? 2 : -2;
    const int nd[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    for (auto d : nd) {
        int f = ff + d[0], r = fr + d[1];
        if (onBoard(f, r) && board_[makeSq(f, r)] == knight) out.push_back(makeSq(f, r));
    }
    const int king = byColor == 0 ? 6 : -6;
    for (int dr = -1; dr <= 1; ++dr)
        for (int df = -1; df <= 1; ++df) {
            if (!dr && !df) continue;
            int f = ff + df, r = fr + dr;
            if (onBoard(f, r) && board_[makeSq(f, r)] == king) out.push_back(makeSq(f, r));
        }
    const int bishop = byColor == 0 ? 3 : -3;
    const int rook = byColor == 0 ? 4 : -4;
    const int queen = byColor == 0 ? 5 : -5;
    const int dirs[8][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int i = 0; i < 8; ++i) {
        int f = ff + dirs[i][0], r = fr + dirs[i][1];
        bool diagonal = i < 4;
        while (onBoard(f, r)) {
            int p = board_[makeSq(f, r)];
            if (p) {
                if (p == queen || p == (diagonal ? bishop : rook)) out.push_back(makeSq(f, r));
                break;
            }
            f += dirs[i][0];
            r += dirs[i][1];
        }
    }
    return out;
}

std::vector<Move> ChessEngine::legalMovesFor(int side) {
    if (side == side_) return legalMoves();
    const int savedSide = side_;
    const int savedEp = ep_;
    side_ = side & 1;
    ep_ = -1;
    std::vector<Move> moves = legalMoves();
    side_ = savedSide;
    ep_ = savedEp;
    recomputeKey();
    return moves;
}

int ChessEngine::recaptureFrom(const Move& cap, int bySide) {
    const int savedSide = side_;
    const int savedEp = ep_;
    const bool flipped = side_ != (bySide & 1);
    if (flipped) {
        side_ = bySide & 1;
        ep_ = -1;
    }
    make(cap);
    int found = -1;
    for (const Move& r : legalMoves()) {
        if (r.to != cap.to) continue;
        found = r.from;
        break;
    }
    unmake();
    if (flipped) {
        side_ = savedSide;
        ep_ = savedEp;
        recomputeKey();
    }
    return found;
}

bool ChessEngine::inCheck(int color) const {
    int king = color == 0 ? 6 : -6;
    for (int s = 0; s < 64; ++s)
        if (board_[s] == king) return attacked(s, 1 - color);
    return true;
}

bool ChessEngine::insufficient() const {
    int minors = 0;
    for (int p : board_) {
        int t = std::abs(p);
        if (t == 1 || t == 4 || t == 5) return false;
        if (t == 2 || t == 3) ++minors;
    }
    return minors <= 1;
}

void ChessEngine::generate(std::vector<Move>& out) const {
    const int dir = side_ == 0 ? 1 : -1;
    const int startRank = side_ == 0 ? 1 : 6;
    const int promoRank = side_ == 0 ? 6 : 1;
    auto own = [&](int p) { return p && ((p > 0) == (side_ == 0)); };
    auto enemy = [&](int p) { return p && ((p > 0) != (side_ == 0)); };
    auto addPromo = [&](int from, int to, int kind) {
        if (rankOf(from) == promoRank && std::abs(board_[from]) == 1) {
            for (int promo : {5, 4, 3, 2}) out.push_back(Move{from, to, promo, kind});
        } else {
            out.push_back(Move{from, to, 0, kind});
        }
    };

    for (int s = 0; s < 64; ++s) {
        int p = board_[s];
        if (!own(p)) continue;
        int t = std::abs(p);
        int f = fileOf(s), r = rankOf(s);
        if (t == 1) {
            int r1 = r + dir;
            if (onBoard(f, r1) && !board_[makeSq(f, r1)]) {
                addPromo(s, makeSq(f, r1), 0);
                if (r == startRank) {
                    int r2 = r + 2 * dir;
                    if (!board_[makeSq(f, r2)]) out.push_back(Move{s, makeSq(f, r2), 0, 0});
                }
            }
            for (int df : {-1, 1}) {
                int nf = f + df;
                if (!onBoard(nf, r1)) continue;
                int to = makeSq(nf, r1);
                if (enemy(board_[to])) addPromo(s, to, 0);
                else if (to == ep_) out.push_back(Move{s, to, 0, 2});
            }
        } else if (t == 2) {
            const int nd[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
            for (auto d : nd) {
                int nf = f + d[0], nr = r + d[1];
                if (!onBoard(nf, nr)) continue;
                int to = makeSq(nf, nr);
                if (!own(board_[to])) out.push_back(Move{s, to, 0, 0});
            }
        } else if (t == 6) {
            for (int dr = -1; dr <= 1; ++dr)
                for (int df = -1; df <= 1; ++df) {
                    if (!dr && !df) continue;
                    int nf = f + df, nr = r + dr;
                    if (!onBoard(nf, nr)) continue;
                    int to = makeSq(nf, nr);
                    if (!own(board_[to])) out.push_back(Move{s, to, 0, 0});
                }
        } else {
            const int dirs[8][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            int begin = (t == 4) ? 4 : 0;
            int end = (t == 3) ? 4 : 8;
            for (int i = begin; i < end; ++i) {
                int nf = f + dirs[i][0], nr = r + dirs[i][1];
                while (onBoard(nf, nr)) {
                    int to = makeSq(nf, nr);
                    if (!board_[to]) out.push_back(Move{s, to, 0, 0});
                    else {
                        if (enemy(board_[to])) out.push_back(Move{s, to, 0, 0});
                        break;
                    }
                    nf += dirs[i][0];
                    nr += dirs[i][1];
                }
            }
        }
    }

    const int opp = 1 - side_;
    if (side_ == 0) {
        if ((castle_ & kWK) && board_[4] == 6 && board_[7] == 4 && !board_[5] && !board_[6] &&
            !attacked(4, opp) && !attacked(5, opp) && !attacked(6, opp))
            out.push_back(Move{4, 6, 0, 1});
        if ((castle_ & kWQ) && board_[4] == 6 && board_[0] == 4 && !board_[1] && !board_[2] && !board_[3] &&
            !attacked(4, opp) && !attacked(3, opp) && !attacked(2, opp))
            out.push_back(Move{4, 2, 0, 1});
    } else {
        if ((castle_ & kBK) && board_[60] == -6 && board_[63] == -4 && !board_[61] && !board_[62] &&
            !attacked(60, opp) && !attacked(61, opp) && !attacked(62, opp))
            out.push_back(Move{60, 62, 0, 1});
        if ((castle_ & kBQ) && board_[60] == -6 && board_[56] == -4 && !board_[57] && !board_[58] && !board_[59] &&
            !attacked(60, opp) && !attacked(59, opp) && !attacked(58, opp))
            out.push_back(Move{60, 58, 0, 1});
    }
}

std::vector<int> ChessEngine::targetsFrom(int sq) {
    std::vector<int> out;
    if (sq < 0 || sq > 63) return out;
    const int p = board_[sq];
    if (!p) return out;
    const int pieceSide = p > 0 ? 0 : 1;
    const int savedSide = side_;
    const int savedEp = ep_;
    if (pieceSide != side_) {
        side_ = pieceSide;
        ep_ = -1;
    }
    for (const Move& m : legalMoves()) {
        if (m.from != sq) continue;
        bool seen = false;
        for (int t : out) {
            if (t == m.to) {
                seen = true;
                break;
            }
        }
        if (!seen) out.push_back(m.to);
    }
    side_ = savedSide;
    ep_ = savedEp;
    if (pieceSide != savedSide) recomputeKey();
    return out;
}

std::vector<Move> ChessEngine::legalMoves() {
    std::vector<Move> pseudo;
    generate(pseudo);
    std::vector<Move> legal;
    legal.reserve(pseudo.size());
    const int me = side_;
    for (const Move& m : pseudo) {
        make(m);
        bool bad = inCheck(me);
        unmake();
        if (!bad) legal.push_back(m);
    }
    return legal;
}

bool ChessEngine::pick(int from, int to, int promo, Move& out) {
    bool anyPromo = false;
    bool found = false;
    for (const Move& m : legalMoves()) {
        if (m.from != from || m.to != to) continue;
        if (m.promo) anyPromo = true;
        if (promo == 0 && m.promo == 0) {
            out = m;
            return true;
        }
        if (promo && m.promo == promo) {
            out = m;
            found = true;
        }
    }
    if (anyPromo && promo == 0) return false;
    return found;
}

void ChessEngine::make(const Move& m) {
    Undo u;
    u.move = m;
    u.moved = board_[m.from];
    u.castle = castle_;
    u.ep = ep_;
    u.half = half_;
    u.full = full_;
    u.side = side_;
    u.captured = board_[m.to];
    u.capSq = m.to;

    board_[m.to] = u.moved;
    board_[m.from] = 0;

    if (m.kind == 2) {
        u.capSq = side_ == 0 ? m.to - 8 : m.to + 8;
        u.captured = board_[u.capSq];
        board_[u.capSq] = 0;
    }
    if (m.promo) board_[m.to] = (side_ == 0 ? 1 : -1) * m.promo;
    if (m.kind == 1) {
        if (m.to == 6) {
            board_[5] = board_[7];
            board_[7] = 0;
        } else if (m.to == 2) {
            board_[3] = board_[0];
            board_[0] = 0;
        } else if (m.to == 62) {
            board_[61] = board_[63];
            board_[63] = 0;
        } else if (m.to == 58) {
            board_[59] = board_[56];
            board_[56] = 0;
        }
    }

    auto clearRook = [&](int sq) {
        if (sq == 7) castle_ &= ~kWK;
        else if (sq == 0) castle_ &= ~kWQ;
        else if (sq == 63) castle_ &= ~kBK;
        else if (sq == 56) castle_ &= ~kBQ;
    };
    clearRook(m.from);
    clearRook(m.to);
    if (u.capSq >= 0) clearRook(u.capSq);
    if (std::abs(u.moved) == 6) castle_ &= ~(side_ == 0 ? (kWK | kWQ) : (kBK | kBQ));

    ep_ = -1;
    if (std::abs(u.moved) == 1 && std::abs(m.to - m.from) == 16) ep_ = (m.from + m.to) / 2;

    if (std::abs(u.moved) == 1 || u.captured) half_ = 0;
    else ++half_;
    if (side_ == 1) ++full_;
    side_ ^= 1;
    history_.push_back(u);
    recomputeKey();
}

void ChessEngine::unmake() {
    Undo u = history_.back();
    history_.pop_back();
    const Move& m = u.move;
    if (m.kind == 1) {
        if (m.to == 6) {
            board_[7] = board_[5];
            board_[5] = 0;
        } else if (m.to == 2) {
            board_[0] = board_[3];
            board_[3] = 0;
        } else if (m.to == 62) {
            board_[63] = board_[61];
            board_[61] = 0;
        } else if (m.to == 58) {
            board_[56] = board_[59];
            board_[59] = 0;
        }
    }
    board_[m.from] = u.moved;
    board_[m.to] = 0;
    if (u.captured) board_[u.capSq] = u.captured;
    castle_ = u.castle;
    ep_ = u.ep;
    half_ = u.half;
    full_ = u.full;
    side_ = u.side;
    recomputeKey();
}

std::string ChessEngine::san(const Move& m) {
    if (m.kind == 1) {
        ChessEngine copy = *this;
        copy.make(m);
        bool check = copy.inCheck(copy.sideToMove());
        bool mate = check && copy.legalMoves().empty();
        std::string s = (m.to == 6 || m.to == 62) ? "O-O" : "O-O-O";
        if (mate) s += '#';
        else if (check) s += '+';
        return s;
    }
    const int t = std::abs(board_[m.from]);
    const bool pawn = t == 1;
    const bool cap = board_[m.to] != 0 || m.kind == 2;
    std::string s;
    const char* letters = " PNBRQK";
    if (!pawn) s += letters[t];
    bool other = false, sameFile = false, sameRank = false;
    for (const Move& o : legalMoves()) {
        if (o.to != m.to || o.from == m.from) continue;
        if (std::abs(board_[o.from]) != t) continue;
        other = true;
        if (fileOf(o.from) == fileOf(m.from)) sameFile = true;
        if (rankOf(o.from) == rankOf(m.from)) sameRank = true;
    }
    if (!pawn && other) {
        if (!sameFile) s += char('a' + fileOf(m.from));
        else if (!sameRank) s += char('1' + rankOf(m.from));
        else {
            s += char('a' + fileOf(m.from));
            s += char('1' + rankOf(m.from));
        }
    }
    if (pawn && cap) s += char('a' + fileOf(m.from));
    if (cap) s += 'x';
    s += char('a' + fileOf(m.to));
    s += char('1' + rankOf(m.to));
    if (m.promo) {
        s += '=';
        s += letters[m.promo];
    }
    ChessEngine copy = *this;
    copy.make(m);
    bool check = copy.inCheck(copy.sideToMove());
    bool mate = check && copy.legalMoves().empty();
    if (mate) s += '#';
    else if (check) s += '+';
    return s;
}

uint64_t ChessEngine::perft(int depth) {
    auto moves = legalMoves();
    if (depth == 1) return moves.size();
    uint64_t n = 0;
    for (const Move& m : moves) {
        make(m);
        n += perft(depth - 1);
        unmake();
    }
    return n;
}

int ChessEngine::chessSelfTest() {
    struct Case {
        const char* fen;
        int depth;
        uint64_t nodes;
    };
    const Case cases[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 1, 20},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 2, 400},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 3, 8902},
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4, 197281},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 1, 48},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 2, 2039},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 1, 14},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 2, 191},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2812},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 1, 44},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 2, 1486},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 1, 6},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 2, 264},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3, 9467},
    };
    int failed = 0;
    for (const Case& c : cases) {
        ChessEngine e;
        if (!e.setFen(c.fen)) {
            std::cerr << "FEN rejected: " << c.fen << "\n";
            ++failed;
            continue;
        }
        if (e.fen() != c.fen) {
            // Halfmove/fullmove spacing is ours; compare board portion only when clocks match.
            // Our writer should round-trip these exact strings.
            std::cerr << "FEN round trip\n  in  " << c.fen << "\n  out " << e.fen() << "\n";
            ++failed;
            continue;
        }
        uint64_t n = e.perft(c.depth);
        if (n != c.nodes) {
            std::cerr << "perft fail depth " << c.depth << " got " << n << " want " << c.nodes << "\n  " << c.fen
                      << "\n";
            if (c.depth <= 2) {
                for (const Move& m : e.legalMoves()) {
                    std::string name = e.san(m);
                    e.make(m);
                    uint64_t sub = c.depth == 1 ? 1 : e.perft(c.depth - 1);
                    e.unmake();
                    std::cerr << "  " << name << " " << sub << "\n";
                }
            }
            ++failed;
        } else {
            std::cout << "perft " << c.depth << " " << n << " ok\n";
        }
    }
    ChessEngine e;
    e.setFen("rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 3");
    Move ep;
    if (!e.pick(36, 43, 0, ep) || ep.kind != 2) {
        std::cerr << "en passant missing\n";
        ++failed;
    } else {
        std::cout << "en passant ok\n";
    }
    auto containsSq = [](const std::vector<int>& v, int sq) {
        for (int s : v)
            if (s == sq) return true;
        return false;
    };
    ChessEngine gaze;
    const char* startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const uint64_t gazeKey = gaze.setFen(startFen) ? gaze.key() : 0;
    const auto a2 = gaze.targetsFrom(8);
    const auto a7 = gaze.targetsFrom(48);
    const auto b8 = gaze.targetsFrom(57);
    if (!gazeKey || a2.size() != 2 || !containsSq(a2, 16) || !containsSq(a2, 24) || a7.size() != 2 ||
        !containsSq(a7, 40) || !containsSq(a7, 32) || b8.size() != 2 || !containsSq(b8, 40) || !containsSq(b8, 42) ||
        gaze.fen() != startFen || gaze.key() != gazeKey) {
        std::cerr << "gaze targets failed\n";
        ++failed;
    } else {
        std::cout << "gaze targets ok\n";
    }
    ChessEngine pinned;
    pinned.setFen("4k3/8/8/8/8/8/4n3/4R3 w - - 0 1");
    if (!pinned.targetsFrom(12).empty() || pinned.sideToMove() != 0) {
        std::cerr << "pinned gaze failed\n";
        ++failed;
    } else {
        std::cout << "pinned gaze ok\n";
    }
    ChessEngine threats;
    const char* hangFen = "8/8/8/2n5/4P3/8/8/4K2k w - - 0 1";
    if (!threats.setFen(hangFen)) {
        std::cerr << "threat fen rejected\n";
        ++failed;
    } else {
        const uint64_t key = threats.key();
        const auto who = threats.attackers(28, 1);
        bool knight = false;
        for (int s : who)
            if (s == 34) knight = true;
        Move cap;
        bool have = false;
        for (const Move& m : threats.legalMovesFor(1)) {
            if (m.from == 34 && m.to == 28) {
                cap = m;
                have = true;
            }
        }
        const int bare = have ? threats.recaptureFrom(cap, 1) : 0;
        if (!knight || !have || bare != -1 || threats.fen() != hangFen || threats.key() != key ||
            threats.sideToMove() != 0) {
            std::cerr << "hanging pawn scan failed\n";
            ++failed;
        } else if (!threats.setFen("8/8/8/2n5/4P3/8/3N4/4K2k w - - 0 1")) {
            std::cerr << "defended fen rejected\n";
            ++failed;
        } else {
            const std::string heldFen = threats.fen();
            const uint64_t heldKey = threats.key();
            Move take;
            bool got = false;
            for (const Move& m : threats.legalMovesFor(1)) {
                if (m.from == 34 && m.to == 28) {
                    take = m;
                    got = true;
                }
            }
            const int back = got ? threats.recaptureFrom(take, 1) : -2;
            if (!got || back != 11 || threats.fen() != heldFen || threats.key() != heldKey) {
                std::cerr << "defended recapture failed\n";
                ++failed;
            } else {
                std::cout << "threat scan ok\n";
            }
        }
    }
    if (failed) std::cerr << failed << " chess test(s) failed\n";
    else std::cout << "chess self-test passed\n";
    return failed ? 1 : 0;
}
