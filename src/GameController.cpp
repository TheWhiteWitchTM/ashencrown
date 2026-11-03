#include "GameController.hpp"

#include "BskyClient.hpp"
#include "ChessAI.hpp"
#include "NatTraverse.hpp"
#include "NetSession.hpp"

#include <QAudioOutput>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHostAddress>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QRandomGenerator>
#include <QSettings>
#include <QSoundEffect>
#include <QDateTime>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {

const char* kSideName[] = {"alliance", "horde"};
const char* kTypeName[] = {"", "pawn", "knight", "bishop", "rook", "queen", "king"};
const char* kAlliance[] = {"", "Footman", "Paladin", "Priest", "Cannon Keep", "Archmage", "Storm King"};
const char* kHorde[] = {"", "Grunt", "Wolf Rider", "Witch Doctor", "Demolisher", "Far Seer", "Warchief"};

QString pieceName(int side, int type) {
    if (type < 1 || type > 6) return "piece";
    return side == 0 ? kAlliance[type] : kHorde[type];
}

QString pieceUrl(int side, int type) {
    if (type < 1 || type > 6 || side < 0 || side > 1) return {};
    return QString("qrc:/assets/pieces/%1_%2.png").arg(kSideName[side], kTypeName[type]);
}

QString styleFor(int side, int type) {
    if (type == 2) return "charge";
    if (type == 4) return "siege";
    if (type == 3) return side == 0 ? "holy" : "shadow";
    if (type == 5) return side == 0 ? "holy" : "fire";
    return "melee";
}

QString fxFor(const QString& style) {
    if (style == "fire") return "qrc:/assets/fx/fire.png";
    if (style == "holy") return "qrc:/assets/fx/holy.png";
    if (style == "shadow") return "qrc:/assets/fx/shadow.png";
    if (style == "siege") return "qrc:/assets/fx/boulder.png";
    return "qrc:/assets/fx/slash.png";
}

QString soundFor(const QString& style) {
    if (style == "charge") return "beast";
    if (style == "siege") return "siege";
    if (style == "holy") return "holy";
    if (style == "shadow" || style == "fire") return "magic";
    return "melee";
}

int pieceValue(int type) {
    switch (type) {
    case 1: return 1;
    case 2: return 3;
    case 3: return 3;
    case 4: return 5;
    case 5: return 9;
    default: return 0;
    }
}

bool tierAllows(int tier, int severity) {
    if (severity < 1 || tier <= 0) return false;
    if (tier >= 2) return true;
    return severity >= 2;
}

QString bark(int as, int at, int ds, int dt) {
    const QString a = pieceName(as, at);
    const QString d = pieceName(ds, dt);
    if (at == 6 && dt == 6) return "Crown meets crown. Only one walks away.";
    if (at == 2 && dt == 1) return a + " runs down the " + d + ".";
    if (at == 5 && dt == 6) return "The spell reaches for the crown.";
    if (at == 4) return a + " brings the wall down on the " + d + ".";
    if (at == 3 && as == 0) return "The Light answers. The " + d + " falls.";
    if (at == 3 && as == 1) return "Hex and bone. The " + d + " breaks.";
    if (at == 5) return a + " unmakes the " + d + ".";
    if (at == 2) return a + " crashes through the " + d + ".";
    if (at == 6) return "The " + a + " does not yield.";
    return a + " strikes down the " + d + ".";
}

bool isPrivateIp(const QString& ip) {
    const QHostAddress a(ip);
    if (a.protocol() != QAbstractSocket::IPv4Protocol) return false;
    const quint32 v = a.toIPv4Address();
    if ((v & 0xFF000000u) == 0x0A000000u) return true;
    if ((v & 0xFFF00000u) == 0xAC100000u) return true;
    if ((v & 0xFFFF0000u) == 0xC0A80000u) return true;
    if ((v & 0xFF000000u) == 0x7F000000u) return true;
    return false;
}

}  // namespace

GameController::GameController(QObject* parent) : QObject(parent) {
    loadSettings();
    net_ = new NetSession(this);
    nat_ = new NatTraverse(this);
    bsky_ = new BskyClient(this);
    bsky_->restore();
    connect(bsky_, &BskyClient::changed, this, [this]() {
        if (mode_ == Mode::Bsky && screen_ == 1 && bsky_ && !bsky_->status().isEmpty())
            statusLine_ = bsky_->status();
        refresh();
    });
    connect(bsky_, &BskyClient::peerReady, this, [this](const QString& did, const QString& handle) { onBskyPeer(did, handle); });
    connect(bsky_, &BskyClient::postMade, this, [this](const QString& tag, const QString& rkey) { onBskyPost(tag, rkey); });
    connect(bsky_, &BskyClient::postFailed, this, [this](const QString&, const QString& why) {
        statusLine_ = why;
        pushLog(why);
        refresh();
    });
    connect(bsky_, &BskyClient::saw, this,
            [this](const QString& did, const QString& op, const QString& rkey, const QString& text, qint64 createdMs, bool live) {
                onBskyRecord(did, op, rkey, text, createdMs, live);
            });
    connect(net_, &NetSession::logLine, this, [this](const QString& line) {
        pushLog(line);
        refresh();
    });
    connect(net_, &NetSession::linkUp, this, [this](int role) { onLink(role); });
    connect(net_, &NetSession::linkDown, this, [this](const QString& why) { onLinkDown(why); });
    connect(net_, &NetSession::connectFailed, this, [this](const QString& why) {
        pushLog(why);
        if (joining_) failJoinStep();
        else refresh();
    });
    connect(net_, &NetSession::messageReceived, this, [this](const QJsonObject& msg) { onMessage(msg); });
    connect(net_, &NetSession::lanFound, this, [this](const QString& name, const QString& ip, int port, int faction, int token) {
        for (int i = 0; i < lanHosts_.size(); ++i) {
            QVariantMap m = lanHosts_[i].toMap();
            if (m["token"].toInt() == token) {
                if (m["name"].toString() == name && m["ip"].toString() == ip && m["port"].toInt() == port &&
                    m["faction"].toInt() == faction)
                    return;
                m["name"] = name;
                m["ip"] = ip;
                m["port"] = port;
                m["faction"] = faction;
                lanHosts_[i] = m;
                refresh();
                return;
            }
        }
        QVariantMap m;
        m["name"] = name;
        m["ip"] = ip;
        m["port"] = port;
        m["faction"] = faction;
        m["token"] = token;
        lanHosts_ << m;
        while (lanHosts_.size() > 12) lanHosts_.removeFirst();
        refresh();
    });
    connect(net_, &NetSession::stunReady, this, [this](bool ok, const QString& ip, int port) {
        if (ok) {
            stunIp_ = ip;
            stunPort_ = port;
            pushLog("The internet sees UDP " + ip + ":" + QString::number(port) + ".");
        } else {
            pushLog("STUN did not answer. UDP punch needs that public port.");
        }
        if (screen_ == 1) rebuildCard();
        refresh();
    });
    connect(nat_, &NatTraverse::logLine, this, [this](const QString& line) {
        pushLog(line);
        refresh();
    });
    connect(nat_, &NatTraverse::finished, this,
            [this](bool tcpOk, bool udpOk, const QString& ip, int tcpPort, int udpPort, const QString& method) {
                tcpOk_ = tcpOk;
                udpOk_ = udpOk;
                natIp_ = ip;
                pubTcp_ = tcpPort;
                pubUdp_ = udpPort;
                natMethod_ = method;
                if (tcpOk || udpOk)
                    pushLog(QString("Router opened a path via %1.").arg(method));
                if (screen_ == 1) rebuildCard();
                refresh();
            });
    initAudio();
    statusLine_ = "The crown waits.";
}

GameController::~GameController() {
    ++thinkGen_;
    if (aiStop_) aiStop_->store(true);
    if (aiThread_.joinable()) aiThread_.join();
}

QString GameController::mode() const {
    switch (mode_) {
    case Mode::Ai: return "ai";
    case Mode::Hotseat: return "hotseat";
    case Mode::Net: return "net";
    case Mode::Bsky: return "bsky";
    case Mode::None: break;
    }
    return {};
}

QString GameController::levelName() const {
    switch (level_) {
    case 0: return "Novice";
    case 1: return "Veteran";
    case 2: return "Warlord";
    default: return "Legend";
    }
}

QString GameController::opponentLine() const {
    if (mode_ == Mode::Ai) return QString("Versus a %1 %2").arg(levelName(), mySide_ == 0 ? "Horde" : "Alliance");
    if (mode_ == Mode::Hotseat) return "Two commanders, one table";
    if (mode_ == Mode::Bsky) {
        if (peerName_.isEmpty()) return "Over Bluesky";
        return "Over Bluesky — @" + peerName_;
    }
    if (peerName_.isEmpty()) return "Across the warfront";
    return "Across the warfront — " + peerName_;
}

QVariantList GameController::squares() const {
    QVariantList list;
    list.reserve(64);
    for (int s = 0; s < 64; ++s) {
        const int p = engine_.pieceAt(s);
        QVariantMap m;
        if (!p) {
            m["type"] = 0;
            m["side"] = -1;
        } else {
            m["type"] = std::abs(p);
            m["side"] = p > 0 ? 0 : 1;
        }
        list << m;
    }
    return list;
}

QVariantList GameController::legalTargets() const {
    QVariantList out;
    for (const Move& m : legal_) {
        if (selected_ < 0 || m.from != selected_) continue;
        if (out.contains(m.to)) continue;
        out << m.to;
    }
    return out;
}

QVariantList GameController::gazeTargets(int sq) const {
    QVariantList out;
    if (sq < 0 || sq > 63) return out;
    const int p = engine_.pieceAt(sq);
    if (!p) return out;
    const int side = p > 0 ? 0 : 1;
    if (side == engine_.sideToMove()) return out;
    ChessEngine copy = engine_;
    const auto targets = copy.targetsFrom(sq);
    for (int to : targets) out << to;
    return out;
}

bool GameController::flipped() const {
    if (mode_ == Mode::Hotseat) return engine_.sideToMove() == 1;
    return mySide_ == 1;
}

bool GameController::inputEnabled() const {
    if (screen_ != 2 || gameOver_ || fightActive_ || promoActive_ || thinking_ || waiting_ || gliding_) return false;
    if (mode_ == Mode::Hotseat) return true;
    return engine_.sideToMove() == mySide_;
}

void GameController::setFaction(int side) {
    hostSide_ = side & 1;
    saveSettings();
    if (screen_ == 1 && !linked_ && mode_ != Mode::Bsky) rebuildCard();
    refresh();
}

void GameController::setLevel(int level) {
    level_ = std::clamp(level, 0, 3);
    saveSettings();
    refresh();
}

void GameController::setPlayerName(const QString& name) {
    const QString trimmed = name.trimmed().left(24);
    if (trimmed.isEmpty() || trimmed == playerName_) return;
    playerName_ = trimmed;
    saveSettings();
    refresh();
}

void GameController::setVolume(qreal volume) {
    volume_ = std::clamp(volume, 0.0, 1.0);
    for (QSoundEffect* s : sounds_) s->setVolume(float(volume_));
    applyMusicVolume();
    saveSettings();
    refresh();
}

void GameController::setFigureOutline(bool on) {
    if (figureOutline_ == on) return;
    figureOutline_ = on;
    saveSettings();
    refresh();
}

void GameController::loadSettings() {
    QSettings s;
    const QByteArray user = qgetenv("USER");
    const QString fallback = user.isEmpty() ? "Wanderer" : QString::fromUtf8(user);
    playerName_ = s.value("name", fallback).toString().left(24);
    if (playerName_.isEmpty()) playerName_ = fallback;
    volume_ = std::clamp(s.value("volume", 0.85).toDouble(), 0.0, 1.0);
    musicOn_ = s.value("music", true).toBool();
    figureOutline_ = s.value("outline", false).toBool();
    hostSide_ = s.value("faction", 0).toInt() & 1;
    level_ = std::clamp(s.value("level", 1).toInt(), 0, 3);
}

void GameController::saveSettings() const {
    QSettings s;
    s.setValue("name", playerName_);
    s.setValue("volume", volume_);
    s.setValue("music", musicOn_);
    s.setValue("outline", figureOutline_);
    s.setValue("faction", hostSide_);
    s.setValue("level", level_);
}

void GameController::initAudio() {
    if (qEnvironmentVariableIsSet("ASHENCROWN_NO_AUDIO")) return;
    const char* names[] = {"click", "move", "melee", "magic", "holy", "beast", "siege",
                           "check", "win", "lose", "hit", "drums", "notify", "sting",
                           "fall_pawn", "fall_knight", "fall_bishop", "fall_rook", "fall_queen", "fall_king"};
    for (const char* name : names) {
        auto* fx = new QSoundEffect(this);
        fx->setSource(QUrl(QString("qrc:/assets/sounds/%1.wav").arg(name)));
        fx->setVolume(float(volume_));
        sounds_.insert(name, fx);
    }
    music_ = new QMediaPlayer(this);
    audio_ = new QAudioOutput(this);
    music_->setAudioOutput(audio_);
    music_->setSource(QUrl("qrc:/assets/sounds/music.wav"));
    music_->setLoops(QMediaPlayer::Infinite);
    applyMusicVolume();
    if (musicOn_) music_->play();
}

void GameController::applyMusicVolume() {
    if (audio_) audio_->setVolume(float(volume_ * 0.22 * musicBed_));
}

void GameController::setMusicBed(qreal bed) {
    musicBed_ = std::clamp(bed, 0.0, 1.0);
    applyMusicVolume();
}

void GameController::stopFightSounds() {
    const char* names[] = {"sting", "fall_pawn", "fall_knight", "fall_bishop", "fall_rook",
                           "fall_queen", "fall_king", "melee", "magic", "holy", "beast", "siege", "hit", "drums"};
    for (const char* name : names) {
        QSoundEffect* fx = sounds_.value(name, nullptr);
        if (fx) fx->stop();
    }
}

QString GameController::coffeeUrl() const {
    return QStringLiteral("https://www.paypal.com/ncp/payment/4P85EQRES3RGU");
}

bool GameController::coffeeEmbedded() const {
#ifdef ASHENCROWN_HAS_WEBENGINE
    return true;
#else
    return false;
#endif
}

void GameController::openCoffee() {
    if (!coffeeEmbedded()) {
        QDesktopServices::openUrl(QUrl(coffeeUrl()));
        return;
    }
    coffeeOpen_ = true;
    refresh();
}

void GameController::closeCoffee() {
    coffeeOpen_ = false;
    refresh();
}

void GameController::playSound(const QString& name) {
    QSoundEffect* fx = sounds_.value(name, nullptr);
    if (!fx) return;
    fx->setVolume(float(volume_));
    fx->play();
}

void GameController::toggleMusic() {
    musicOn_ = !musicOn_;
    if (music_) {
        if (musicOn_) music_->play();
        else music_->pause();
    }
    saveSettings();
    refresh();
}

void GameController::refresh() {
    legal_ = engine_.legalMoves();
    checkSquare_ = -1;
    if (engine_.inCheck(engine_.sideToMove())) {
        const int want = engine_.sideToMove() == 0 ? 6 : -6;
        for (int s = 0; s < 64; ++s) {
            if (engine_.pieceAt(s) == want) checkSquare_ = s;
        }
    }
    rebuildHints();
    emit tick();
}

int GameController::hintTier() const {
    if (mode_ != Mode::Ai || screen_ != 2 || gameOver_ || mySide_ < 0) return 0;
    if (level_ <= 0) return 3;
    if (level_ == 1) return 2;
    if (level_ == 2) return 1;
    return 0;
}

QVariantMap GameController::coachMove(const Move& m) const {
    QVariantMap out;
    const int human = mySide_;
    if (human < 0) return out;
    const int enemy = 1 - human;
    ChessEngine sim = engine_;
    sim.make(m);
    const int landedType = std::abs(sim.pieceAt(m.to));
    const QString moverName = pieceName(human, landedType);
    int cheapest = 100;
    int cheapType = 0;
    bool loose = false;
    int defenderSq = -1;
    QVariantList marks;
    const std::vector<Move> replies = sim.legalMoves();
    for (const Move& r : replies) {
        if (r.to != m.to || sim.pieceAt(m.to) == 0) continue;
        const int at = std::abs(sim.pieceAt(r.from));
        const int landed = r.promo ? r.promo : at;
        const int val = pieceValue(landed);
        if (!marks.contains(r.from)) marks << r.from;
        if (val < cheapest) {
            cheapest = val;
            cheapType = at;
        }
        ChessEngine reply = sim;
        const int back = reply.recaptureFrom(r, sim.sideToMove());
        if (back < 0) loose = true;
        else if (defenderSq < 0) defenderSq = back;
    }
    int severity = -1;
    QString line;
    if (!marks.isEmpty() && landedType > 0 && landedType != 6) {
        if (loose) severity = 2;
        else if (cheapest < pieceValue(landedType)) severity = 1;
        if (severity >= 2) {
            line = QString("The %1 would fall to the %2. Nothing takes it back.")
                       .arg(moverName, pieceName(enemy, cheapType));
        } else if (severity == 1) {
            const int defType = defenderSq >= 0 ? std::abs(sim.pieceAt(defenderSq)) : 0;
            const QString defenderName = defType ? pieceName(human, defType) : QString("answer");
            line = QString("The %1 can take the %2 there. The %3 answers, and the trade still costs you.")
                       .arg(pieceName(enemy, cheapType), moverName, defenderName);
        }
    }
    QString hungName;
    int hungFrom = -1;
    for (const Move& r : replies) {
        int victimSq = r.to;
        if (r.kind == 2) victimSq = r.to + (sim.sideToMove() == 0 ? -8 : 8);
        if (victimSq == m.to) continue;
        const int victim = sim.pieceAt(victimSq);
        if (!victim || (victim > 0 ? 0 : 1) != human || std::abs(victim) == 6) continue;
        if (threatSev_[victimSq] >= 2) continue;
        ChessEngine reply = sim;
        if (reply.recaptureFrom(r, sim.sideToMove()) >= 0) continue;
        hungName = pieceName(human, std::abs(victim));
        hungFrom = r.from;
        break;
    }
    if (!hungName.isEmpty() && severity < 2) {
        severity = 2;
        line = QString("This step leaves the %1 loose.").arg(hungName);
        marks.clear();
        if (hungFrom >= 0) marks << hungFrom;
    }
    if (severity < 1 || line.isEmpty()) return out;
    out["show"] = true;
    out["severity"] = severity;
    out["line"] = line;
    out["marks"] = marks;
    out["sq"] = m.to;
    return out;
}

void GameController::rebuildHints() {
    looseMarks_.clear();
    cautionMarks_.clear();
    for (int i = 0; i < 64; ++i) {
        looseMarks_ << 0;
        cautionMarks_ << 0;
        threatSev_[i] = 0;
        threatLine_[i].clear();
        threatMarks_[i].clear();
        moveOn_[i] = false;
        moveSev_[i] = -1;
        moveLine_[i].clear();
        moveMarks_[i].clear();
    }
    const int tier = hintTier();
    if (tier <= 0) return;
    const int human = mySide_;
    const int enemy = 1 - human;
    ChessEngine scan = engine_;
    std::vector<Move> incoming[64];
    for (const Move& m : scan.legalMovesFor(enemy)) {
        int victimSq = m.to;
        if (m.kind == 2) victimSq = m.to + (enemy == 0 ? -8 : 8);
        if (victimSq < 0 || victimSq > 63) continue;
        const int victim = engine_.pieceAt(victimSq);
        if (!victim || (victim > 0 ? 0 : 1) != human || std::abs(victim) == 6) continue;
        incoming[victimSq].push_back(m);
    }
    for (int sq = 0; sq < 64; ++sq) {
        if (incoming[sq].empty()) continue;
        const int victim = std::abs(engine_.pieceAt(sq));
        const int victimVal = pieceValue(victim);
        int cheapest = 100;
        int cheapType = 0;
        bool loose = false;
        int defenderSq = -1;
        QVariantList marks;
        for (const Move& m : incoming[sq]) {
            const int mover = std::abs(engine_.pieceAt(m.from));
            const int landed = m.promo ? m.promo : mover;
            const int val = pieceValue(landed);
            if (!marks.contains(m.from)) marks << m.from;
            if (val < cheapest) {
                cheapest = val;
                cheapType = mover;
            }
            ChessEngine sim = engine_;
            const int back = sim.recaptureFrom(m, enemy);
            if (back < 0) loose = true;
            else if (defenderSq < 0) defenderSq = back;
        }
        int severity = 0;
        if (loose) severity = 2;
        else if (cheapest < victimVal) severity = 1;
        const QString attackerName = pieceName(enemy, cheapType);
        const QString victimName = pieceName(human, victim);
        QString extra;
        if (marks.size() == 2) extra = " Another can reach it too.";
        else if (marks.size() > 2) extra = " Others can reach it too.";
        QString line;
        if (severity >= 2) {
            line = QString("The %1 can take this %2. Nothing takes it back.%3").arg(attackerName, victimName, extra);
        } else if (severity == 1) {
            const int defType = defenderSq >= 0 ? std::abs(engine_.pieceAt(defenderSq)) : 0;
            const QString defenderName = defType ? pieceName(human, defType) : QString("answer");
            line = QString("The %1 can take this %2. The %3 can answer, and the trade still costs you.%4")
                       .arg(attackerName, victimName, defenderName, extra);
        }
        threatSev_[sq] = severity;
        threatLine_[sq] = line;
        threatMarks_[sq] = marks;
        if (tier >= 3 && severity >= 1) looseMarks_[sq] = severity;
    }
    if (selected_ < 0 || engine_.sideToMove() != human) return;
    for (const Move& m : legal_) {
        if (m.from != selected_) continue;
        const QVariantMap advice = coachMove(m);
        if (advice.isEmpty()) continue;
        const int sev = advice.value("severity").toInt();
        if (moveOn_[m.to] && sev <= moveSev_[m.to]) continue;
        moveOn_[m.to] = true;
        moveSev_[m.to] = sev;
        moveLine_[m.to] = advice.value("line").toString();
        moveMarks_[m.to] = advice.value("marks").toList();
        if (tier >= 3) cautionMarks_[m.to] = sev;
    }
}

QVariantMap GameController::coachAt(int sq) const {
    QVariantMap out;
    if (sq < 0 || sq > 63) return out;
    const int tier = hintTier();
    if (tier <= 0) return out;
    auto pack = [&](int severity, const QString& line, const QVariantList& marks) {
        QVariantMap m;
        m["show"] = true;
        m["severity"] = severity;
        m["line"] = line;
        m["marks"] = tier >= 2 ? marks : QVariantList();
        m["sq"] = sq;
        return m;
    };
    if (moveOn_[sq] && tierAllows(tier, moveSev_[sq]))
        return pack(moveSev_[sq], moveLine_[sq], moveMarks_[sq]);
    const int piece = engine_.pieceAt(sq);
    const int side = piece > 0 ? 0 : (piece < 0 ? 1 : -1);
    if (piece && side == mySide_ && std::abs(piece) != 6 && tierAllows(tier, threatSev_[sq]))
        return pack(threatSev_[sq], threatLine_[sq], threatMarks_[sq]);
    return out;
}

void GameController::startAi(int side, int aiLevel) {
    shutdownNet();
    stopThink();
    mode_ = Mode::Ai;
    mySide_ = side & 1;
    level_ = std::clamp(aiLevel, 0, 3);
    hostSide_ = mySide_;
    gameLive_ = true;
    saveSettings();
    freshPosition();
    if (engine_.sideToMove() != mySide_) QTimer::singleShot(280, this, [this]() { startThink(); });
}

void GameController::startHotseat() {
    shutdownNet();
    stopThink();
    mode_ = Mode::Hotseat;
    mySide_ = -1;
    gameLive_ = true;
    freshPosition();
}

void GameController::freshPosition() {
    engine_.reset();
    keys_.clear();
    keys_.push_back(engine_.key());
    moves_.clear();
    chatLines_.clear();
    allianceSpoils_.clear();
    hordeSpoils_.clear();
    selected_ = -1;
    lastFrom_ = lastTo_ = -1;
    gameOver_ = false;
    resultText_.clear();
    fightActive_ = false;
    fight_.clear();
    promoActive_ = false;
    gliding_ = false;
    waiting_ = false;
    drawOffer_ = false;
    pending_ = {};
    screen_ = 2;
    statusLine_ = "Alliance to move.";
    refresh();
}

void GameController::clickSquare(int sq) {
    if (!inputEnabled() || sq < 0 || sq > 63) return;
    const int p = engine_.pieceAt(sq);
    const int side = p > 0 ? 0 : (p < 0 ? 1 : -1);
    const int stm = engine_.sideToMove();
    if (selected_ < 0 || (side == stm && sq != selected_)) {
        if (side != stm) {
            selected_ = -1;
            refresh();
            return;
        }
        selected_ = sq;
        playSound("click");
        refresh();
        return;
    }
    if (sq == selected_) {
        selected_ = -1;
        refresh();
        return;
    }
    const int piece = engine_.pieceAt(selected_);
    const int rank = sq / 8;
    const bool promo = std::abs(piece) == 1 && ((piece > 0 && rank == 7) || (piece < 0 && rank == 0));
    Move m;
    if (promo) {
        bool any = false;
        for (int type : {5, 4, 3, 2}) {
            if (engine_.pick(selected_, sq, type, m)) any = true;
        }
        if (!any) return;
        promoFrom_ = selected_;
        promoTo_ = sq;
        promoActive_ = true;
        refresh();
        return;
    }
    if (!engine_.pick(selected_, sq, 0, m)) return;
    commitMove(m);
}

void GameController::choosePromo(int type) {
    if (!promoActive_ || type < 2 || type > 5) return;
    Move m;
    if (!engine_.pick(promoFrom_, promoTo_, type, m)) {
        promoActive_ = false;
        refresh();
        return;
    }
    promoActive_ = false;
    commitMove(m);
}

void GameController::cancelPromo() {
    promoActive_ = false;
    refresh();
}

void GameController::commitMove(const Move& m) {
    if (mode_ == Mode::Bsky) {
        if (!bskyLive_ || engine_.sideToMove() != mySide_) return;
        applyLocal(m, false);
        bskyPostMove(m);
        return;
    }
    if (mode_ == Mode::Net && roleLocked_ && !roleHost_) {
        if (!net_ || !linked_) return;
        waiting_ = true;
        QJsonObject o;
        o["t"] = "req";
        o["from"] = m.from;
        o["to"] = m.to;
        o["promo"] = m.promo;
        o["seq"] = ++seq_;
        net_->send(o);
        statusLine_ = "The order is crossing the warfront.";
        selected_ = -1;
        refresh();
        return;
    }
    applyLocal(m, mode_ == Mode::Net && roleHost_);
}

void GameController::applyLocal(const Move& m, bool tell) {
    if (gameOver_) return;
    drawOffer_ = false;
    const int mover = engine_.sideToMove();
    const int attacker = engine_.pieceAt(m.from);
    int captured = engine_.pieceAt(m.to);
    if (m.kind == 2) captured = engine_.pieceAt(m.to + (mover == 0 ? -8 : 8));
    const QString notation = QString::fromStdString(engine_.san(m));
    engine_.make(m);
    keys_.push_back(engine_.key());
    moves_ << notation;
    lastFrom_ = m.from;
    lastTo_ = m.to;
    selected_ = -1;
    waiting_ = false;
    if (captured) {
        if (mover == 0) allianceSpoils_ << std::abs(captured);
        else hordeSpoils_ << std::abs(captured);
    }
    if (tell) sendMoveMsg(m, notation);
    int rookFrom = -1;
    int rookTo = -1;
    if (captured) {
        beginFight(attacker, captured);
        gliding_ = false;
    } else {
        if (m.kind == 1) {
            if (m.to == 6) {
                rookFrom = 7;
                rookTo = 5;
            } else if (m.to == 2) {
                rookFrom = 0;
                rookTo = 3;
            } else if (m.to == 62) {
                rookFrom = 63;
                rookTo = 61;
            } else if (m.to == 58) {
                rookFrom = 56;
                rookTo = 59;
            }
        }
        gliding_ = true;
        playSound("move");
        QTimer::singleShot(860, this, [this]() {
            if (gliding_) onMotionDone();
        });
    }
    refresh();
    if (!captured) emit glide(m.from, m.to, rookFrom, rookTo);
}

void GameController::beginFight(int attacker, int defender) {
    const int at = std::abs(attacker);
    const int dt = std::abs(defender);
    const int as = attacker > 0 ? 0 : 1;
    const int ds = defender > 0 ? 0 : 1;
    const QString style = styleFor(as, at);
    QVariantMap f;
    f["attackerSide"] = as;
    f["defenderSide"] = ds;
    f["attackerType"] = at;
    f["defenderType"] = dt;
    f["attackerName"] = pieceName(as, at);
    f["defenderName"] = pieceName(ds, dt);
    f["attackerUrl"] = pieceUrl(as, at);
    f["defenderUrl"] = pieceUrl(ds, dt);
    f["style"] = style;
    f["fx"] = fxFor(style);
    f["impact"] = "qrc:/assets/fx/impact.png";
    f["bark"] = bark(as, at, ds, dt);
    fight_ = f;
    fightActive_ = true;
    playSound(soundFor(style));
}

void GameController::fightFinished() {
    if (!fightActive_) return;
    fightActive_ = false;
    onMotionDone();
}

void GameController::onMotionDone() {
    gliding_ = false;
    fightActive_ = false;
    if (pending_.on) {
        Pending p = pending_;
        pending_.on = false;
        applyLocal(p.m, p.tell);
        if (p.bskyPly >= 0) bskyAck(p.bskyPly);
        return;
    }
    finishTurn();
}

void GameController::finishTurn() {
    if (gameOver_) return;
    const auto legal = engine_.legalMoves();
    const int stm = engine_.sideToMove();
    const bool check = engine_.inCheck(stm);
    if (legal.empty()) {
        if (check) {
            if (stm == 0) endGame("Checkmate. The Horde takes the crown.", 1);
            else endGame("Checkmate. The Alliance holds the crown.", 0);
        } else {
            endGame("Stalemate. Neither crown falls.", -1);
        }
        return;
    }
    if (engine_.halfmoveClock() >= 100) {
        endGame("Fifty moves without a capture or a pawn. The field goes still.", -1);
        return;
    }
    int same = 0;
    const uint64_t key = engine_.key();
    for (uint64_t x : keys_)
        if (x == key) ++same;
    if (same >= 3) {
        endGame("The same position three times. Neither crown can be forced.", -1);
        return;
    }
    if (engine_.insufficient()) {
        endGame("Too little remains to take a crown.", -1);
        return;
    }
    if (check) {
        playSound("check");
        statusLine_ = stm == 0 ? "The Storm King is in check." : "The Warchief is in check.";
    } else {
        statusLine_ = stm == 0 ? "Alliance to move." : "Horde to move.";
    }
    refresh();
    if (mode_ == Mode::Ai && stm != mySide_) {
        QTimer::singleShot(280, this, [this]() {
            if (!gameOver_ && !fightActive_ && !gliding_ && screen_ == 2) startThink();
        });
    }
}

void GameController::endGame(const QString& text, int winner) {
    gameOver_ = true;
    resultText_ = text;
    statusLine_ = text;
    thinking_ = false;
    ++thinkGen_;
    if (aiStop_) aiStop_->store(true);
    if (winner < 0) playSound("notify");
    else if (mySide_ < 0 || winner == mySide_) playSound("win");
    else playSound("lose");
    refresh();
}

void GameController::startThink() {
    if (mode_ != Mode::Ai || gameOver_ || fightActive_ || gliding_ || screen_ != 2) return;
    if (engine_.sideToMove() == mySide_) return;
    stopThink();
    thinking_ = true;
    statusLine_ = (engine_.sideToMove() == 0 ? QString("The Alliance") : QString("The Horde")) + " is weighing the field.";
    const int gen = thinkGen_;
    auto stop = std::make_shared<std::atomic<bool>>(false);
    aiStop_ = stop;
    const int depth = level_ == 0 ? 2 : level_ == 1 ? 3 : level_ == 2 ? 4 : 5;
    const int ms = level_ == 0 ? 400 : level_ == 1 ? 800 : level_ == 2 ? 1800 : 3500;
    const int level = level_;
    ChessEngine snapshot = engine_;
    refresh();
    aiThread_ = std::thread([this, stop, gen, depth, ms, level, snapshot]() mutable {
        auto moves = snapshot.legalMoves();
        if (moves.empty()) return;
        Move chosen;
        if (level == 0 && QRandomGenerator::global()->bounded(100) < 35)
            chosen = moves.at(QRandomGenerator::global()->bounded(int(moves.size())));
        else {
            ChessAI ai;
            chosen = ai.think(snapshot, depth, ms, stop.get());
        }
        QMetaObject::invokeMethod(
            this,
            [this, gen, chosen]() {
                if (gen != thinkGen_) return;
                thinking_ = false;
                onAiMove(chosen);
            },
            Qt::QueuedConnection);
    });
}

void GameController::stopThink() {
    ++thinkGen_;
    if (aiStop_) aiStop_->store(true);
    if (aiThread_.joinable()) aiThread_.join();
    thinking_ = false;
}

void GameController::onAiMove(const Move& incoming) {
    if (mode_ != Mode::Ai || gameOver_ || screen_ != 2) return;
    if (engine_.sideToMove() == mySide_) return;
    Move m = incoming;
    if (!engine_.pick(m.from, m.to, m.promo, m)) {
        if (!engine_.pick(m.from, m.to, 5, m)) {
            const auto moves = engine_.legalMoves();
            if (moves.empty()) {
                finishTurn();
                return;
            }
            m = moves.front();
        }
    }
    if (fightActive_ || gliding_) {
        pending_ = Pending{m, false, true};
        return;
    }
    applyLocal(m, false);
}

void GameController::resign() {
    if (screen_ != 2 || gameOver_) return;
    if (mode_ == Mode::Bsky && bskyLive_)
        postBsky("resign", QString("AC1 RESIGN %1\nThe crown is yielded.").arg(tableNonce_));
    if (mode_ == Mode::Net && net_ && linked_) {
        QJsonObject o;
        o["t"] = "resign";
        net_->send(o);
    }
    const int stm = mode_ == Mode::Hotseat ? engine_.sideToMove() : mySide_;
    const int winner = stm == 0 ? 1 : 0;
    endGame(stm == 0 ? "The Alliance yields the crown." : "The Horde yields the crown.", winner);
}

void GameController::offerDraw() {
    if (screen_ != 2 || gameOver_ || mode_ == Mode::Ai) return;
    if (mode_ == Mode::Hotseat) {
        drawOffer_ = true;
        statusLine_ = "A draw is offered to the other commander.";
        refresh();
        return;
    }
    if (mode_ == Mode::Bsky && bskyLive_) {
        postBsky("draw", QString("AC1 DRAW %1 offer\nA draw is offered.").arg(tableNonce_));
        statusLine_ = "Draw offered.";
        refresh();
        return;
    }
    if (mode_ == Mode::Net && net_ && linked_) {
        QJsonObject o;
        o["t"] = "draw";
        o["what"] = "offer";
        net_->send(o);
        statusLine_ = "Draw offered.";
        refresh();
    }
}

void GameController::answerDraw(bool accept) {
    if (!drawOffer_) return;
    drawOffer_ = false;
    if (mode_ == Mode::Bsky && bskyLive_)
        postBsky("draw", QString("AC1 DRAW %1 %2\nThe draw is answered.").arg(tableNonce_, accept ? "yes" : "no"));
    if (mode_ == Mode::Net && net_ && linked_) {
        QJsonObject o;
        o["t"] = "draw";
        o["what"] = accept ? "accept" : "decline";
        net_->send(o);
    }
    if (accept) endGame("Both sides lower their arms. The crown is shared.", -1);
    else {
        statusLine_ = "The draw was declined.";
        refresh();
    }
}

void GameController::chat(const QString& text) {
    const QString line = text.trimmed().left(180);
    if (line.isEmpty() || (mode_ != Mode::Net && mode_ != Mode::Bsky)) return;
    chatLines_ << (playerName_ + ": " + line);
    while (chatLines_.size() > 40) chatLines_.removeFirst();
    if (mode_ == Mode::Bsky && bskyLive_) {
        const QString clean = line;
        QString said = clean;
        said.replace('\n', ' ');
        postBsky("chat", QString("AC1 CHAT %1 %2").arg(tableNonce_, said));
    }
    if (net_ && linked_) {
        QJsonObject o;
        o["t"] = "chat";
        o["name"] = playerName_;
        o["text"] = line;
        net_->send(o);
    }
    refresh();
}

void GameController::rematch() {
    if (screen_ != 2 || !gameOver_) return;
    if (mode_ == Mode::Net && net_ && linked_) {
        QJsonObject o;
        o["t"] = "rematch";
        net_->send(o);
    }
    if (mode_ == Mode::Bsky && bsky_)
        postBsky("rematch", QString("AC1 REMATCH %1\nThe table is set again.").arg(tableNonce_));
    applyRematch();
}

void GameController::applyRematch() {
    const int side = mySide_;
    freshPosition();
    mySide_ = side;
    if (mode_ == Mode::Bsky) {
        bskyTaken_.clear();
        lastAckPly_ = -1;
        moveRkey_.clear();
        ackRkey_.clear();
        theirMoveRkey_.clear();
    }
    if (mode_ == Mode::Ai && engine_.sideToMove() != mySide_)
        QTimer::singleShot(280, this, [this]() { startThink(); });
}

void GameController::backToMenu() {
    if (screen_ == 2 && mode_ == Mode::Net && !gameOver_ && net_ && linked_) {
        QJsonObject o;
        o["t"] = "resign";
        net_->send(o);
    }
    if (screen_ == 2 && mode_ == Mode::Bsky && !gameOver_ && bskyLive_)
        postBsky("resign", QString("AC1 RESIGN %1\nThe crown is yielded.").arg(tableNonce_));
    stopThink();
    shutdownNet();
    if (bsky_) bsky_->unwatch();
    bskyLive_ = false;
    bskyOpponentDid_.clear();
    tableNonce_.clear();
    screen_ = 0;
    gameOver_ = false;
    fightActive_ = false;
    promoActive_ = false;
    waiting_ = false;
    gliding_ = false;
    resultText_.clear();
    statusLine_ = "The crown waits.";
    refresh();
}

void GameController::copyText(const QString& text) {
    if (auto* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        if (QClipboard* clip = gui->clipboard()) clip->setText(text);
    }
    pushLog("Warcode copied.");
    refresh();
}

void GameController::shutdownNet() {
    joining_ = false;
    ++joinGen_;
    linked_ = false;
    gameLive_ = false;
    roleHost_ = false;
    roleLocked_ = false;
    if (net_) net_->stop();
    if (nat_) nat_->clear();
}

void GameController::enterLobby(int side) {
    stopThink();
    shutdownNet();
    hostSide_ = side & 1;
    mySide_ = hostSide_;
    mode_ = Mode::Net;
    screen_ = 1;
    gameOver_ = false;
    peerName_.clear();
    netLines_.clear();
    lanHosts_.clear();
    stunIp_.clear();
    stunPort_ = 0;
    natIp_.clear();
    tcpOk_ = udpOk_ = false;
    pubTcp_ = pubUdp_ = 0;
    token_ = quint16(QRandomGenerator::global()->bounded(1, 65535));
    saveSettings();
    net_->start(playerName_, hostSide_, token_);
    nat_->begin(net_->localIp(), net_->tcpPort(), net_->udpPort());
    rebuildCard();
    statusLine_ = "You hold the field. Share the warcode. The other commander marches to you.";
    pushLog(statusLine_);
    refresh();
}

void GameController::rebuildCard() {
    if (!net_) return;
    WarCard c;
    c.token = token_;
    c.faction = quint8(hostSide_ & 1);
    const QString local = net_->localIp();
    c.lan = QHostAddress(local);
    c.lanPort = net_->tcpPort();
    if (isPrivateIp(local)) c.flags = quint8(c.flags | 4);
    const QString pubIp = !natIp_.isEmpty() ? natIp_ : stunIp_;
    if (!pubIp.isEmpty()) c.pub = QHostAddress(pubIp);
    if (tcpOk_ && pubTcp_ > 0) {
        c.flags = quint8(c.flags | 1);
        c.pubTcp = quint16(pubTcp_);
    }
    if (udpOk_ && pubUdp_ > 0) {
        c.flags = quint8(c.flags | 2);
        c.pubUdp = quint16(pubUdp_);
    } else if (stunPort_ > 0 && !stunIp_.isEmpty()) {
        c.flags = quint8(c.flags | 2);
        c.pubUdp = quint16(stunPort_);
    }
    warcode_ = encodeWarcard(c);
    QStringList bits;
    if (c.flags & 4) bits << QString("LAN %1:%2").arg(local).arg(net_->tcpPort());
    if (tcpOk_ && !natIp_.isEmpty())
        bits << QString("Public %1:%2 via %3").arg(natIp_).arg(pubTcp_).arg(natMethod_.isEmpty() ? QString("UPnP") : natMethod_);
    if ((c.flags & 2) && !pubIp.isEmpty()) bits << QString("UDP punch %1:%2").arg(pubIp).arg(c.pubUdp);
    if (bits.isEmpty())
        bits << "The router has not opened a port yet. Same-LAN play still works. Over the internet, forward TCP or trade warcodes. A symmetric NAT can block the punch; a VPN such as Tailscale is the sure path.";
    endpointLabel_ = bits.join("\n");
}

void GameController::pushLog(const QString& line) {
    if (line.isEmpty()) return;
    netLines_ << line;
    while (netLines_.size() > 40) netLines_.removeFirst();
}

void GameController::march(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty() || !net_ || linked_) return;
    joining_ = true;
    joinStep_ = 0;
    directHost_.clear();
    WarCard card;
    if (decodeWarcard(t, card)) {
        card_ = card;
        const QString fac = card.faction ? "Horde" : "Alliance";
        statusLine_ = "Marching on a " + fac + " warcode.";
        pushLog(statusLine_);
        tryNextJoin();
        refresh();
        return;
    }
    QString host = t;
    quint16 port = 41777;
    const int colon = t.lastIndexOf(':');
    if (colon > 0 && !t.contains("::")) {
        bool ok = false;
        const uint p = t.mid(colon + 1).toUInt(&ok);
        if (ok && p > 0 && p < 65536) {
            host = t.left(colon);
            port = quint16(p);
        }
    }
    directHost_ = host;
    directPort_ = port;
    joinStep_ = 10;
    statusLine_ = QString("Marching to %1:%2.").arg(host).arg(port);
    pushLog(statusLine_);
    net_->connectToHost(host, port);
    armJoin(6000);
    refresh();
}

void GameController::joinLan(const QString& ip, int port) { march(ip + ":" + QString::number(port)); }

void GameController::armJoin(int ms) {
    const int gen = ++joinGen_;
    QTimer::singleShot(ms, this, [this, gen]() {
        if (gen != joinGen_ || !joining_) return;
        failJoinStep();
    });
}

void GameController::failJoinStep() {
    if (!joining_ || advancing_) return;
    advancing_ = true;
    ++joinGen_;
    if (net_) net_->abortConnect();
    if (joinStep_ >= 10) {
        joining_ = false;
        advancing_ = false;
        statusLine_ = "Could not reach that address.";
        pushLog(statusLine_);
        refresh();
        return;
    }
    ++joinStep_;
    advancing_ = false;
    tryNextJoin();
}

void GameController::tryNextJoin() {
    if (!joining_ || !net_) return;
    if (joinStep_ <= 0 && (card_.flags & 4) && card_.lanPort && !card_.lan.isNull()) {
        joinStep_ = 0;
        statusLine_ = "Trying the road on their own network.";
        pushLog(statusLine_ + " " + card_.lan.toString() + ":" + QString::number(card_.lanPort));
        net_->connectToHost(card_.lan.toString(), card_.lanPort);
        armJoin(1800);
        refresh();
        return;
    }
    if (joinStep_ <= 1 && (card_.flags & 1) && card_.pubTcp && !card_.pub.isNull()) {
        joinStep_ = 1;
        statusLine_ = "Trying the public gate.";
        pushLog(statusLine_ + " " + card_.pub.toString() + ":" + QString::number(card_.pubTcp));
        net_->connectToHost(card_.pub.toString(), card_.pubTcp);
        armJoin(6000);
        refresh();
        return;
    }
    if (joinStep_ <= 2 && card_.pubUdp && !card_.pub.isNull()) {
        joinStep_ = 2;
        statusLine_ = "Punching through the wall.";
        pushLog(statusLine_ + " " + card_.pub.toString() + ":" + QString::number(card_.pubUdp));
        net_->punchToward(card_.pub.toString(), card_.pubUdp);
        armJoin(10000);
        refresh();
        return;
    }
    joining_ = false;
    statusLine_ = "The march failed. A symmetric NAT can block a punch. Forward TCP " + QString::number(net_->tcpPort()) +
                  ", or share a VPN address such as Tailscale and march to host:port.";
    pushLog(statusLine_);
    refresh();
}

void GameController::sendHello() {
    if (!net_) return;
    QJsonObject o;
    o["t"] = "hello";
    o["v"] = 1;
    o["name"] = playerName_;
    o["token"] = int(token_);
    o["faction"] = hostSide_;
    net_->send(o);
}

void GameController::onLink(int roleHint) {
    ++joinGen_;
    joining_ = false;
    advancing_ = false;
    linked_ = true;
    if (roleHint == 1) {
        roleHost_ = true;
        roleLocked_ = true;
    } else if (roleHint == 0) {
        roleHost_ = false;
        roleLocked_ = true;
    } else {
        roleHost_ = false;
        roleLocked_ = false;
    }
    pushLog(roleLocked_ ? (roleHost_ ? "Link up. You are the host." : "Link up. You are marching in.")
                        : "Link up. Agreeing who hosts.");
    if (!(roleLocked_ && roleHost_)) sendHello();
    statusLine_ = roleHost_ && roleLocked_ ? "A commander is at the gate." : "The road is open.";
    refresh();
}

void GameController::onLinkDown(const QString& why) {
    const bool was = linked_;
    linked_ = false;
    pushLog(why);
    if (joining_) {
        failJoinStep();
        return;
    }
    if (was && screen_ == 2 && mode_ == Mode::Net && !gameOver_) endGame("The warfront went dark. " + why, -1);
    else refresh();
}

void GameController::onMessage(const QJsonObject& msg) {
    const QString t = msg.value("t").toString();
    if (t == "hello") handleHello(msg);
    else if (t == "welcome") handleWelcome(msg);
    else if (t == "req") handleReq(msg);
    else if (t == "move") handleMove(msg);
    else if (t == "reject") {
        waiting_ = false;
        statusLine_ = msg.value("why").toString();
        if (statusLine_.isEmpty()) statusLine_ = "The march was refused.";
        refresh();
    } else if (t == "resign") {
        if (!gameOver_) endGame("The far commander yields the field.", mySide_ < 0 ? engine_.sideToMove() : mySide_);
    } else if (t == "draw") handleDraw(msg);
    else if (t == "chat") {
        chatLines_ << (msg.value("name").toString() + ": " + msg.value("text").toString());
        while (chatLines_.size() > 40) chatLines_.removeFirst();
        playSound("notify");
        refresh();
    } else if (t == "rematch") {
        if (gameOver_) applyRematch();
    }
}

void GameController::handleHello(const QJsonObject& msg) {
    peerName_ = msg.value("name").toString();
    const int peerTok = msg.value("token").toInt();
    if (!roleLocked_) {
        roleHost_ = int(token_) < peerTok || (int(token_) == peerTok && playerName_ < peerName_);
        roleLocked_ = true;
    } else if (!roleHost_ && int(token_) < peerTok) {
        // Both sides marched at once. The lower token takes the host chair.
        roleHost_ = true;
    }
    if (roleHost_ && !gameLive_) openMatch();
    else refresh();
}

void GameController::handleWelcome(const QJsonObject& msg) {
    if (gameLive_) return;
    peerName_ = msg.value("name").toString();
    roleHost_ = false;
    roleLocked_ = true;
    beginGame(msg.value("you").toInt(), msg.value("fen").toString());
}

void GameController::openMatch() {
    if (gameLive_ || !net_) return;
    engine_.reset();
    QJsonObject o;
    o["t"] = "welcome";
    o["v"] = 1;
    o["name"] = playerName_;
    o["you"] = 1 - (hostSide_ & 1);
    o["fen"] = QString::fromStdString(engine_.fen());
    net_->send(o);
    beginGame(hostSide_, QString::fromStdString(engine_.fen()));
}

void GameController::beginGame(int mySide, const QString& fen) {
    if (gameLive_) return;
    stopThink();
    gameLive_ = true;
    mySide_ = mySide;
    if (mode_ != Mode::Bsky) mode_ = Mode::Net;
    if (fen.isEmpty() || !engine_.setFen(fen.toStdString())) engine_.reset();
    keys_.clear();
    keys_.push_back(engine_.key());
    moves_.clear();
    chatLines_.clear();
    allianceSpoils_.clear();
    hordeSpoils_.clear();
    selected_ = -1;
    lastFrom_ = lastTo_ = -1;
    gameOver_ = false;
    resultText_.clear();
    fightActive_ = false;
    fight_.clear();
    promoActive_ = false;
    gliding_ = false;
    waiting_ = false;
    drawOffer_ = false;
    pending_ = {};
    screen_ = 2;
    statusLine_ = engine_.sideToMove() == 0 ? "Alliance to move." : "Horde to move.";
    pushLog("The battle begins.");
    refresh();
}

void GameController::handleReq(const QJsonObject& msg) {
    if (!roleHost_ || gameOver_) {
        reject("The battle is not waiting on you.");
        return;
    }
    if (engine_.sideToMove() == mySide_) {
        reject("It is not your turn.");
        return;
    }
    Move m;
    if (!engine_.pick(msg.value("from").toInt(), msg.value("to").toInt(), msg.value("promo").toInt(), m)) {
        reject("That march is illegal.");
        return;
    }
    seq_ = msg.value("seq").toInt();
    if (fightActive_ || gliding_) {
        pending_ = Pending{m, true, true};
        return;
    }
    applyLocal(m, true);
}

void GameController::handleMove(const QJsonObject& msg) {
    if (roleHost_ || gameOver_) return;
    waiting_ = false;
    Move m;
    const int from = msg.value("from").toInt();
    const int to = msg.value("to").toInt();
    const int promo = msg.value("promo").toInt();
    if (!engine_.pick(from, to, promo, m)) {
        const QString fen = msg.value("fen").toString();
        if (!fen.isEmpty()) engine_.setFen(fen.toStdString());
        keys_.clear();
        keys_.push_back(engine_.key());
        const QString notation = msg.value("san").toString();
        if (!notation.isEmpty()) moves_ << notation;
        gliding_ = false;
        fightActive_ = false;
        finishTurn();
        return;
    }
    if (fightActive_ || gliding_) {
        pending_ = Pending{m, false, true};
        return;
    }
    applyLocal(m, false);
}

void GameController::handleDraw(const QJsonObject& msg) {
    const QString what = msg.value("what").toString();
    if (what == "offer") {
        drawOffer_ = true;
        statusLine_ = (peerName_.isEmpty() ? "The far commander" : peerName_) + " offers a draw.";
        playSound("notify");
        refresh();
    } else if (what == "accept") {
        drawOffer_ = false;
        endGame("Both sides lower their arms. The crown is shared.", -1);
    } else {
        drawOffer_ = false;
        statusLine_ = "The draw was declined.";
        refresh();
    }
}

void GameController::sendMoveMsg(const Move& m, const QString& notation) {
    if (!net_ || !linked_) return;
    QJsonObject o;
    o["t"] = "move";
    o["from"] = m.from;
    o["to"] = m.to;
    o["promo"] = m.promo;
    o["seq"] = seq_;
    o["fen"] = QString::fromStdString(engine_.fen());
    o["san"] = notation;
    net_->send(o);
}

void GameController::reject(const QString& why) {
    if (!net_ || !linked_) return;
    QJsonObject o;
    o["t"] = "reject";
    o["why"] = why;
    net_->send(o);
    pushLog(why);
}

bool GameController::bskyLoggedIn() const { return bsky_ && bsky_->loggedIn(); }
bool GameController::bskyBusy() const { return bsky_ && bsky_->busy(); }
QString GameController::bskyHandle() const { return bsky_ ? bsky_->handle() : QString(); }

QString GameController::freshNonce() const {
    return QString::number(QRandomGenerator::global()->generate(), 16).rightJustified(8, QLatin1Char('0'));
}

void GameController::enterBsky(int side) {
    stopThink();
    shutdownNet();
    if (bsky_) bsky_->unwatch();
    hostSide_ = side & 1;
    mySide_ = hostSide_;
    mode_ = Mode::Bsky;
    screen_ = 1;
    gameOver_ = false;
    bskyLive_ = false;
    peerName_.clear();
    bskyOpponentDid_.clear();
    netLines_.clear();
    saveSettings();
    if (bsky_ && bsky_->loggedIn())
        statusLine_ = "Signed in as @" + bsky_->handle() + ". Type their handle and sit.";
    else
        statusLine_ = "Sign in with Bluesky. A browser window will ask you to allow posts.";
    pushLog(statusLine_);
    refresh();
}

void GameController::bskyLogin(const QString& handle) {
    if (bsky_) bsky_->login(handle);
}

void GameController::bskyLogout() {
    if (!bsky_) return;
    bsky_->logout();
    bskyLive_ = false;
    statusLine_ = "Signed out of Bluesky.";
    refresh();
}

void GameController::bskySit(const QString& opponent) {
    if (!bsky_ || !bsky_->loggedIn()) {
        statusLine_ = "Sign in with Bluesky first.";
        refresh();
        return;
    }
    QString opp = opponent.trimmed();
    if (opp.startsWith('@')) opp = opp.mid(1);
    if (opp.isEmpty()) {
        statusLine_ = "Type their Bluesky handle.";
        refresh();
        return;
    }
    bskyTaken_.clear();
    lastAckPly_ = -1;
    bskyLive_ = false;
    tableNonce_.clear();
    myNonce_ = freshNonce();
    moveRkey_.clear();
    ackRkey_.clear();
    sitRkey_.clear();
    startRkey_.clear();
    theirMoveRkey_.clear();
    peerName_.clear();
    bskyOpponentDid_.clear();
    statusLine_ = "Looking up @" + opp + ".";
    pushLog(statusLine_);
    refresh();
    bsky_->watch(opp);
}

void GameController::postBsky(const QString& tag, const QString& line) {
    if (!bsky_ || tableNonce_.isEmpty()) return;
    bsky_->createPost(tag, line);
}

void GameController::dropBsky(const QString& rkey) {
    if (bsky_ && !rkey.isEmpty()) bsky_->deletePost(rkey);
}

void GameController::postSit(const QString& nonce) {
    if (!bsky_) return;
    if (!sitRkey_.isEmpty()) {
        dropBsky(sitRkey_);
        sitRkey_.clear();
    }
    const QString name = playerName_.isEmpty() ? bsky_->handle() : playerName_;
    bsky_->createPost("sit", QString("AC1 SIT %1 %2 %3\nSitting down at the Ashen Crown table.")
                                 .arg(nonce)
                                 .arg(hostSide_ & 1)
                                 .arg(name));
}

void GameController::onBskyPost(const QString& tag, const QString& rkey) {
    if (tag == "move") moveRkey_ = rkey;
    else if (tag == "ack") ackRkey_ = rkey;
    else if (tag == "sit") {
        sitRkey_ = rkey;
        if (bskyLive_) {
            dropBsky(sitRkey_);
            sitRkey_.clear();
        }
    } else if (tag == "start") {
        startRkey_ = rkey;
        QTimer::singleShot(25000, this, [this, rkey]() {
            if (startRkey_ == rkey) {
                dropBsky(startRkey_);
                startRkey_.clear();
            }
        });
    } else if (tag == "chat" || tag == "draw" || tag == "resign" || tag == "rematch") {
        QTimer::singleShot(20000, this, [this, rkey]() { dropBsky(rkey); });
    }
}

void GameController::bskyPostMove(const Move& m) {
    const int ply = moves_.size() - 1;
    const QString san = moves_.isEmpty() ? QString() : moves_.last();
    postBsky("move", QString("AC1 MOVE %1 %2 %3 %4 %5\n%6").arg(tableNonce_).arg(ply).arg(m.from).arg(m.to).arg(m.promo).arg(san));
}

void GameController::bskyAck(int ply) {
    if (ply < 0 || ply == lastAckPly_ || tableNonce_.isEmpty()) return;
    lastAckPly_ = ply;
    postBsky("ack", QString("AC1 ACK %1 %2\nSeen.").arg(tableNonce_).arg(ply));
    QTimer::singleShot(8000, this, [this, ply]() {
        if (lastAckPly_ == ply && !ackRkey_.isEmpty()) {
            dropBsky(ackRkey_);
            ackRkey_.clear();
        }
    });
}

void GameController::beginBsky(int mySide) {
    if (bskyLive_) return;
    bskyLive_ = true;
    bskyTaken_.clear();
    lastAckPly_ = -1;
    theirMoveRkey_.clear();
    dropBsky(sitRkey_);
    sitRkey_.clear();
    beginGame(mySide, QString());
}

void GameController::onBskyPeer(const QString& did, const QString& handle) {
    if (mode_ != Mode::Bsky || !bsky_) return;
    bskyOpponentDid_ = did;
    peerName_ = handle;
    bskyHost_ = bsky_->did() < did;
    if (bskyHost_) {
        tableNonce_ = myNonce_;
        postSit(myNonce_);
        statusLine_ = "You hold the table. Waiting for @" + handle + " to sit.";
    } else {
        statusLine_ = "Watching @" + handle + ". Waiting for them to open the table.";
    }
    pushLog(statusLine_);
    refresh();
}

void GameController::onBskyRecord(const QString& did, const QString& op, const QString& rkey, const QString& text, qint64 createdMs, bool live) {
    if (mode_ != Mode::Bsky || did != bskyOpponentDid_) return;
    if (op == "delete") {
        if (rkey == theirMoveRkey_) {
            theirMoveRkey_.clear();
            if (!ackRkey_.isEmpty()) {
                dropBsky(ackRkey_);
                ackRkey_.clear();
            }
        }
        return;
    }
    const bool stale = !live && createdMs > 0 && QDateTime::currentMSecsSinceEpoch() - createdMs > 30LL * 60 * 1000;
    const BskyNote note = parseBskyNote(text);
    if (!note.ok) return;
    if (stale && (note.verb == "SIT" || note.verb == "START")) return;

    if (note.verb == "SIT") {
        if (!note.text.isEmpty()) peerName_ = note.text;
        if (bskyHost_) {
            if (note.nonce != myNonce_ || bskyLive_) return;
            const int guest = 1 - (hostSide_ & 1);
            postBsky("start", QString("AC1 START %1 %2\nThe table is set.").arg(myNonce_).arg(guest));
            beginBsky(hostSide_ & 1);
            return;
        }
        if (bskyLive_ || tableNonce_ == note.nonce) return;
        tableNonce_ = note.nonce;
        postSit(note.nonce);
        statusLine_ = "They opened the table. Sitting down.";
        pushLog(statusLine_);
        refresh();
        return;
    }

    if (note.nonce != tableNonce_ || tableNonce_.isEmpty()) return;
    if (note.verb == "START") {
        if (!bskyHost_ && !bskyLive_) beginBsky(note.side);
        return;
    }
    if (!bskyLive_) return;
    if (note.verb == "MOVE") {
        if (bskyTaken_.contains(note.ply) || note.ply < moves_.size()) {
            if (note.ply == moves_.size() - 1) bskyAck(note.ply);
            return;
        }
        if (note.ply != moves_.size() || engine_.sideToMove() == mySide_) return;
        if (pending_.on && pending_.bskyPly == note.ply) return;
        Move m;
        if (!engine_.pick(note.from, note.to, note.promo, m)) {
            pushLog("Their order did not fit the board.");
            refresh();
            return;
        }
        bskyTaken_.insert(note.ply);
        theirMoveRkey_ = rkey;
        if (!moveRkey_.isEmpty()) {
            dropBsky(moveRkey_);
            moveRkey_.clear();
        }
        if (!startRkey_.isEmpty()) {
            dropBsky(startRkey_);
            startRkey_.clear();
        }
        if (fightActive_ || gliding_) {
            pending_ = Pending{m, false, true, note.ply};
            return;
        }
        applyLocal(m, false);
        bskyAck(note.ply);
        return;
    }
    if (note.verb == "ACK") {
        if (note.ply == moves_.size() - 1 && !moveRkey_.isEmpty()) {
            dropBsky(moveRkey_);
            moveRkey_.clear();
        }
        return;
    }
    if (note.verb == "RESIGN") {
        if (!gameOver_) endGame("The far commander yields the field.", mySide_ < 0 ? engine_.sideToMove() : mySide_);
        return;
    }
    if (note.verb == "DRAW") {
        QJsonObject o;
        o["what"] = note.text == "offer" ? "offer" : note.text == "yes" ? "accept" : "decline";
        handleDraw(o);
        return;
    }
    if (note.verb == "CHAT") {
        const QString who = peerName_.isEmpty() ? QString("Them") : peerName_;
        chatLines_ << (who + ": " + note.text);
        while (chatLines_.size() > 40) chatLines_.removeFirst();
        playSound("notify");
        refresh();
        return;
    }
    if (note.verb == "REMATCH" && gameOver_) applyRematch();
}

int GameController::coachSelfTest() {
    QCoreApplication::setOrganizationName(QStringLiteral("AshenCrown"));
    QCoreApplication::setApplicationName(QStringLiteral("AshenCrownCoachTest"));
    qputenv("ASHENCROWN_NO_AUDIO", "1");
    GameController g;
    int failed = 0;
    auto expect = [&](bool ok, const char* msg) {
        if (!ok) {
            std::cerr << msg << "\n";
            ++failed;
        }
    };
    auto load = [&](const char* fen, int side, int level) {
        g.mode_ = Mode::Ai;
        g.mySide_ = side;
        g.level_ = level;
        g.screen_ = 2;
        g.gameOver_ = false;
        g.selected_ = -1;
        g.engine_.setFen(fen);
        g.refresh();
    };

    load("8/8/8/2n5/4P3/8/8/4K2k w - - 0 1", 0, 0);
    expect(g.hintTier() == 3, "novice tier");
    expect(g.looseMarks_.value(28).toInt() == 2, "hanging pawn marked");
    const QVariantMap hang = g.coachAt(28);
    expect(hang.value("show").toBool(), "hanging pawn speaks");
    expect(hang.value("severity").toInt() == 2, "hanging severity");
    expect(hang.value("line").toString().contains("Wolf Rider"), "names the rider");
    expect(hang.value("line").toString().contains("Nothing takes it back"), "says it is loose");
    expect(hang.value("marks").toList().contains(34), "marks the rider");

    load("8/8/8/2n5/4P3/8/3N4/4K2k w - - 0 1", 0, 0);
    expect(g.looseMarks_.value(28).toInt() == 0, "held pawn is not marked");
    expect(!g.coachAt(28).value("show").toBool(), "held pawn stays quiet");
    g.selected_ = 11;
    g.refresh();
    const QVariantMap left = g.coachAt(5);
    expect(left.value("severity").toInt() == 2, "stepping away hangs the footman");
    expect(left.value("line").toString().contains("Footman"), "names the piece left loose");
    expect(left.value("line").toString().contains("loose"), "says the piece is loose");

    load("8/8/8/3p4/4Q3/5P2/8/4K2k w - - 0 1", 0, 0);
    expect(g.looseMarks_.value(28).toInt() == 1, "bad trade marked");
    const QVariantMap trade = g.coachAt(28);
    expect(trade.value("severity").toInt() == 1, "trade severity");
    expect(trade.value("line").toString().contains("trade"), "trade words");
    expect(trade.value("line").toString().contains("Footman"), "names the defender");

    g.level_ = 2;
    g.refresh();
    expect(g.hintTier() == 1, "warlord tier");
    expect(g.looseMarks_.value(28).toInt() == 0, "warlord does not mark the trade");
    expect(!g.coachAt(28).value("show").toBool(), "warlord is silent on a trade");

    load("8/8/8/2n5/4P3/8/8/4K2k w - - 0 1", 0, 2);
    expect(g.coachAt(28).value("show").toBool(), "warlord names a loose piece");
    expect(g.coachAt(28).value("marks").toList().isEmpty(), "warlord draws no squares");
    expect(g.looseMarks_.value(28).toInt() == 0, "warlord leaves the board unmarked");

    g.level_ = 3;
    g.refresh();
    expect(g.hintTier() == 0, "legend tier");
    expect(!g.coachAt(28).value("show").toBool(), "legend is silent");

    load("8/8/8/2n5/8/8/4P3/4K2k w - - 0 1", 0, 0);
    g.selected_ = 12;
    g.refresh();
    const QVariantMap step = g.coachAt(28);
    expect(step.value("show").toBool() && step.value("severity").toInt() == 2, "e4 hangs the footman");
    expect(g.cautionMarks_.value(28).toInt() == 2, "training marks the bad square");
    expect(!g.coachAt(20).value("show").toBool(), "e3 is quiet");
    expect(g.cautionMarks_.value(20).toInt() == 0, "e3 is not marked");

    g.level_ = 1;
    g.refresh();
    expect(g.hintTier() == 2, "veteran tier");
    expect(g.coachAt(28).value("show").toBool(), "veteran warns on the bad step");
    expect(!g.coachAt(28).value("marks").toList().isEmpty(), "veteran still points at the threat");
    expect(g.cautionMarks_.value(28).toInt() == 0, "veteran does not paint the square ahead of the hover");

    load("8/8/8/2p5/4N3/8/8/4K2k b - - 0 1", 1, 0);
    expect(g.looseMarks_.value(34).toInt() == 2, "horde pawn is marked");
    expect(g.coachAt(34).value("line").toString().contains("Paladin"), "names the paladin");

    g.mode_ = Mode::Hotseat;
    g.mySide_ = -1;
    g.refresh();
    expect(g.hintTier() == 0, "a shared table has no coach");
    expect(!g.coachAt(34).value("show").toBool(), "hotseat stays silent");

    if (failed) {
        std::cerr << failed << " coach test(s) failed\n";
        return 1;
    }
    std::cout << "coach ok\n";
    return 0;
}
