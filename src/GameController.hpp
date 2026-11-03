#pragma once

#include "BskyClient.hpp"
#include "ChessEngine.hpp"
#include "NetCode.hpp"

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QtGlobal>

#include <array>
#include <atomic>
#include <memory>
#include <thread>

class NetSession;
class NatTraverse;
class QAudioOutput;
class QMediaPlayer;
class QSoundEffect;

// Board, fights, AI, and the Alliance/Horde warfront.
// Alliance is side 0 and moves first. Horde is side 1.
class GameController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int screen READ screen NOTIFY tick)
    Q_PROPERTY(QString mode READ mode NOTIFY tick)
    Q_PROPERTY(int mySide READ mySide NOTIFY tick)
    Q_PROPERTY(int faction READ faction WRITE setFaction NOTIFY tick)
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY tick)
    Q_PROPERTY(QString levelName READ levelName NOTIFY tick)
    Q_PROPERTY(int hintTier READ hintTier NOTIFY tick)
    Q_PROPERTY(QVariantList looseMarks READ looseMarks NOTIFY tick)
    Q_PROPERTY(QVariantList cautionMarks READ cautionMarks NOTIFY tick)
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY tick)
    Q_PROPERTY(QString peerName READ peerName NOTIFY tick)
    Q_PROPERTY(QString opponentLine READ opponentLine NOTIFY tick)
    Q_PROPERTY(QVariantList squares READ squares NOTIFY tick)
    Q_PROPERTY(int selected READ selected NOTIFY tick)
    Q_PROPERTY(QVariantList legalTargets READ legalTargets NOTIFY tick)
    Q_PROPERTY(bool flipped READ flipped NOTIFY tick)
    Q_PROPERTY(int turn READ turn NOTIFY tick)
    Q_PROPERTY(bool thinking READ thinking NOTIFY tick)
    Q_PROPERTY(bool inputEnabled READ inputEnabled NOTIFY tick)
    Q_PROPERTY(bool gameOver READ gameOver NOTIFY tick)
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY tick)
    Q_PROPERTY(QString resultText READ resultText NOTIFY tick)
    Q_PROPERTY(QStringList moves READ moves NOTIFY tick)
    Q_PROPERTY(bool fightActive READ fightActive NOTIFY tick)
    Q_PROPERTY(QVariantMap fight READ fight NOTIFY tick)
    Q_PROPERTY(bool promoActive READ promoActive NOTIFY tick)
    Q_PROPERTY(int promoSide READ promoSide NOTIFY tick)
    Q_PROPERTY(int checkSquare READ checkSquare NOTIFY tick)
    Q_PROPERTY(int lastFrom READ lastFrom NOTIFY tick)
    Q_PROPERTY(int lastTo READ lastTo NOTIFY tick)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY tick)
    Q_PROPERTY(bool musicOn READ musicOn NOTIFY tick)
    Q_PROPERTY(bool figureOutline READ figureOutline WRITE setFigureOutline NOTIFY tick)
    Q_PROPERTY(QStringList netLines READ netLines NOTIFY tick)
    Q_PROPERTY(QString warcode READ warcode NOTIFY tick)
    Q_PROPERTY(QString endpointLabel READ endpointLabel NOTIFY tick)
    Q_PROPERTY(QVariantList lanHosts READ lanHosts NOTIFY tick)
    Q_PROPERTY(bool waiting READ waiting NOTIFY tick)
    Q_PROPERTY(bool joining READ joining NOTIFY tick)
    Q_PROPERTY(QStringList chatLines READ chatLines NOTIFY tick)
    Q_PROPERTY(bool drawOffer READ drawOffer NOTIFY tick)
    Q_PROPERTY(QVariantList allianceSpoils READ allianceSpoils NOTIFY tick)
    Q_PROPERTY(QVariantList hordeSpoils READ hordeSpoils NOTIFY tick)
    Q_PROPERTY(bool coffeeOpen READ coffeeOpen NOTIFY tick)
    Q_PROPERTY(bool coffeeEmbedded READ coffeeEmbedded CONSTANT)
    Q_PROPERTY(QString coffeeUrl READ coffeeUrl CONSTANT)
    Q_PROPERTY(bool bskyTable READ bskyTable NOTIFY tick)
    Q_PROPERTY(bool bskyLoggedIn READ bskyLoggedIn NOTIFY tick)
    Q_PROPERTY(bool bskyBusy READ bskyBusy NOTIFY tick)
    Q_PROPERTY(QString bskyHandle READ bskyHandle NOTIFY tick)

public:
    explicit GameController(QObject* parent = nullptr);
    ~GameController() override;

    int screen() const { return screen_; }
    QString mode() const;
    int mySide() const { return mySide_; }
    int faction() const { return hostSide_; }
    int level() const { return level_; }
    QString levelName() const;
    int hintTier() const;
    QVariantList looseMarks() const { return looseMarks_; }
    QVariantList cautionMarks() const { return cautionMarks_; }
    QString playerName() const { return playerName_; }
    QString peerName() const { return peerName_; }
    QString opponentLine() const;
    QVariantList squares() const;
    int selected() const { return selected_; }
    QVariantList legalTargets() const;
    bool flipped() const;
    int turn() const { return engine_.sideToMove(); }
    bool thinking() const { return thinking_; }
    bool inputEnabled() const;
    bool gameOver() const { return gameOver_; }
    QString statusLine() const { return statusLine_; }
    QString resultText() const { return resultText_; }
    QStringList moves() const { return moves_; }
    bool fightActive() const { return fightActive_; }
    QVariantMap fight() const { return fight_; }
    bool promoActive() const { return promoActive_; }
    int promoSide() const { return engine_.sideToMove(); }
    int checkSquare() const { return checkSquare_; }
    int lastFrom() const { return lastFrom_; }
    int lastTo() const { return lastTo_; }
    qreal volume() const { return volume_; }
    bool musicOn() const { return musicOn_; }
    bool figureOutline() const { return figureOutline_; }
    QStringList netLines() const { return netLines_; }
    QString warcode() const { return warcode_; }
    QString endpointLabel() const { return endpointLabel_; }
    QVariantList lanHosts() const { return lanHosts_; }
    bool waiting() const { return waiting_; }
    bool joining() const { return joining_; }
    QStringList chatLines() const { return chatLines_; }
    bool drawOffer() const { return drawOffer_; }
    QVariantList allianceSpoils() const { return allianceSpoils_; }
    QVariantList hordeSpoils() const { return hordeSpoils_; }
    bool coffeeOpen() const { return coffeeOpen_; }
    bool coffeeEmbedded() const;
    QString coffeeUrl() const;
    bool bskyTable() const { return mode_ == Mode::Bsky && screen_ == 1; }
    bool bskyLoggedIn() const;
    bool bskyBusy() const;
    QString bskyHandle() const;

    void setFaction(int side);
    void setLevel(int level);
    void setPlayerName(const QString& name);
    void setVolume(qreal volume);
    void setFigureOutline(bool on);

    Q_INVOKABLE void startAi(int side, int level);
    Q_INVOKABLE void startHotseat();
    Q_INVOKABLE void enterLobby(int side);
    Q_INVOKABLE void enterBsky(int side);
    Q_INVOKABLE void bskyLogin(const QString& handle);
    Q_INVOKABLE void bskyLogout();
    Q_INVOKABLE void bskySit(const QString& opponent);
    Q_INVOKABLE void march(const QString& code);
    Q_INVOKABLE void joinLan(const QString& ip, int port);
    Q_INVOKABLE void clickSquare(int sq);
    Q_INVOKABLE QVariantList gazeTargets(int sq) const;
    Q_INVOKABLE void choosePromo(int type);
    Q_INVOKABLE void cancelPromo();
    Q_INVOKABLE void fightFinished();
    Q_INVOKABLE void resign();
    Q_INVOKABLE void offerDraw();
    Q_INVOKABLE void answerDraw(bool accept);
    Q_INVOKABLE void chat(const QString& text);
    Q_INVOKABLE void backToMenu();
    Q_INVOKABLE void rematch();
    Q_INVOKABLE void copyText(const QString& text);
    Q_INVOKABLE void toggleMusic();
    Q_INVOKABLE void playSound(const QString& name);
    Q_INVOKABLE void stopFightSounds();
    Q_INVOKABLE void setMusicBed(qreal bed);
    Q_INVOKABLE QVariantMap coachAt(int sq) const;
    Q_INVOKABLE void openCoffee();
    Q_INVOKABLE void closeCoffee();

    static int coachSelfTest();

signals:
    void tick();
    void glide(int from, int to, int rookFrom, int rookTo);

private:
    enum class Mode { None, Ai, Hotseat, Net, Bsky };

    struct Pending {
        Move m;
        bool tell = false;
        bool on = false;
        int bskyPly = -1;
    };

    void refresh();
    void rebuildHints();
    void applyMusicVolume();
    QVariantMap coachMove(const Move& m) const;
    void saveSettings() const;
    void loadSettings();
    void initAudio();
    void freshPosition();
    void commitMove(const Move& m);
    void applyLocal(const Move& m, bool tell);
    void finishTurn();
    void endGame(const QString& text, int winner);
    void beginFight(int attacker, int defender);
    void onMotionDone();
    void startThink();
    void stopThink();
    void onAiMove(const Move& incoming);
    void shutdownNet();
    void rebuildCard();
    void pushLog(const QString& line);
    void armJoin(int ms);
    void tryNextJoin();
    void failJoinStep();
    void sendHello();
    void openMatch();
    void beginGame(int mySide, const QString& fen);
    void applyRematch();
    void onLink(int roleHint);
    void onLinkDown(const QString& why);
    void onMessage(const QJsonObject& msg);
    void handleHello(const QJsonObject& msg);
    void handleWelcome(const QJsonObject& msg);
    void handleReq(const QJsonObject& msg);
    void handleMove(const QJsonObject& msg);
    void handleDraw(const QJsonObject& msg);
    void sendMoveMsg(const Move& m, const QString& notation);
    void reject(const QString& why);
    void onBskyPeer(const QString& did, const QString& handle);
    void onBskyPost(const QString& tag, const QString& rkey);
    void onBskyRecord(const QString& did, const QString& op, const QString& rkey, const QString& text, qint64 createdMs, bool live);
    void postSit(const QString& nonce);
    void postBsky(const QString& tag, const QString& line);
    void bskyPostMove(const Move& m);
    void bskyAck(int ply);
    void beginBsky(int mySide);
    void dropBsky(const QString& rkey);
    QString freshNonce() const;

    ChessEngine engine_;
    std::vector<Move> legal_;
    std::vector<uint64_t> keys_;
    Mode mode_ = Mode::None;
    int screen_ = 0;
    int mySide_ = 0;
    int hostSide_ = 0;
    int level_ = 1;
    int selected_ = -1;
    int lastFrom_ = -1;
    int lastTo_ = -1;
    int checkSquare_ = -1;
    int seq_ = 0;
    bool thinking_ = false;
    bool gameOver_ = false;
    bool fightActive_ = false;
    bool promoActive_ = false;
    bool gliding_ = false;
    bool waiting_ = false;
    bool joining_ = false;
    bool advancing_ = false;
    bool drawOffer_ = false;
    bool gameLive_ = false;
    bool linked_ = false;
    bool roleHost_ = false;
    bool roleLocked_ = false;
    bool musicOn_ = true;
    bool figureOutline_ = false;
    bool coffeeOpen_ = false;
    int promoFrom_ = -1;
    int promoTo_ = -1;
    int joinStep_ = 0;
    int joinGen_ = 0;
    int thinkGen_ = 0;
    qreal volume_ = 0.85;
    qreal musicBed_ = 1;
    QVariantList looseMarks_;
    QVariantList cautionMarks_;
    std::array<int, 64> threatSev_{};
    std::array<QString, 64> threatLine_{};
    std::array<QVariantList, 64> threatMarks_{};
    std::array<bool, 64> moveOn_{};
    std::array<int, 64> moveSev_{};
    std::array<QString, 64> moveLine_{};
    std::array<QVariantList, 64> moveMarks_{};
    QString playerName_;
    QString peerName_;
    QString statusLine_;
    QString resultText_;
    QStringList moves_;
    QStringList chatLines_;
    QStringList netLines_;
    QVariantList allianceSpoils_;
    QVariantList hordeSpoils_;
    QVariantList lanHosts_;
    QVariantMap fight_;
    QString warcode_;
    QString endpointLabel_;
    QString natIp_;
    QString natMethod_;
    QString stunIp_;
    QString directHost_;
    int pubTcp_ = 0;
    int pubUdp_ = 0;
    int stunPort_ = 0;
    quint16 directPort_ = 0;
    quint16 token_ = 0;
    bool tcpOk_ = false;
    bool udpOk_ = false;
    Pending pending_;
    WarCard card_;

    std::shared_ptr<std::atomic<bool>> aiStop_;
    std::thread aiThread_;

    NetSession* net_ = nullptr;
    NatTraverse* nat_ = nullptr;
    BskyClient* bsky_ = nullptr;
    QString bskyOpponentDid_;
    QString tableNonce_;
    QString myNonce_;
    QString moveRkey_;
    QString ackRkey_;
    QString sitRkey_;
    QString startRkey_;
    QString theirMoveRkey_;
    bool bskyHost_ = false;
    bool bskyLive_ = false;
    int lastAckPly_ = -1;
    QSet<int> bskyTaken_;
    QMediaPlayer* music_ = nullptr;
    QAudioOutput* audio_ = nullptr;
    QHash<QString, QSoundEffect*> sounds_;
};
