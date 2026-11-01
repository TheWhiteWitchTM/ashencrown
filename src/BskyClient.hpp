#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

// A public Bluesky post that carries one Ashen Crown order.
// The first line is the order. Anything after it is for a person reading the post.
struct BskyNote {
    bool ok = false;
    QString verb;
    QString nonce;
    int side = 0;
    int ply = 0;
    int from = 0;
    int to = 0;
    int promo = 0;
    QString text;
};

BskyNote parseBskyNote(const QString& postText);
int bskySelfTest();

// OAuth sign-in, public posts, and a firehose watch on one account.
// Moves are ordinary posts. The other client reads them off the firehose and
// the author deletes the post once that client has taken the move.
class BskyClient : public QObject {
    Q_OBJECT
public:
    explicit BskyClient(QObject* parent = nullptr);
    ~BskyClient() override;

    void restore();
    bool loggedIn() const { return loggedIn_; }
    bool busy() const { return busy_; }
    QString handle() const { return handle_; }
    QString did() const { return did_; }
    QString status() const { return status_; }

    void login(const QString& handle);
    void logout();

    void createPost(const QString& tag, const QString& text);
    void deletePost(const QString& rkey);
    void watch(const QString& handleOrDid);
    void unwatch();

signals:
    void changed();
    void peerReady(const QString& did, const QString& handle);
    void postMade(const QString& tag, const QString& rkey);
    void postFailed(const QString& tag, const QString& why);
    void saw(const QString& did, const QString& op, const QString& rkey, const QString& text, qint64 createdMs, bool live);

private:
    struct Key;
    struct Job {
        bool create = true;
        QString tag;
        QString text;
        QString rkey;
    };

    void setStatus(const QString& text);
    void failLogin(const QString& why);
    void saveSession() const;
    void clearSession();
    QString proof(const QString& method, const QString& url, const QString& access) const;
    void request(const QString& method, const QUrl& url, const QByteArray& body, const QByteArray& contentType, bool authed, bool retry, const std::function<void(int, const QByteArray&, const QByteArray&)>& done);
    void beginResolve(const QString& who, const std::function<void(bool, const QString&, const QString&, const QString&)>& done);
    void sendPar(int gen, const QString& scope, bool fallback);
    void pump();
    void openSocket();
    void backfill();
    void armReconnect();

    friend int bskySelfTest();

    bool loggedIn_ = false;
    bool busy_ = false;
    bool watching_ = false;
    int watchGen_ = 0;
    int loginGen_ = 0;
    QString handle_;
    QString did_;
    QString pds_;
    QString issuer_;
    QString authEndpoint_;
    QString tokenEndpoint_;
    QString parEndpoint_;
    QString clientId_;
    QString redirect_;
    QString access_;
    QString refresh_;
    QString nonce_;
    QString status_;
    QString verifier_;
    QString state_;
    qint64 expiresAt_ = 0;

    QString watchDid_;
    int jet_ = 0;
    int backoffMs_ = 1500;

    class QNetworkAccessManager* net_ = nullptr;
    class QTcpServer* server_ = nullptr;
    class QWebSocket* socket_ = nullptr;
    class QTimer* reconnect_ = nullptr;
    Key* key_ = nullptr;
    QByteArray priorKey_;
    QList<Job> queue_;
    bool pumping_ = false;
};
