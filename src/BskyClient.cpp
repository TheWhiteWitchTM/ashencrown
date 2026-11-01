#include "BskyClient.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <QWebSocket>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>

namespace {

constexpr char kScope[] = "atproto repo:app.bsky.feed.post?action=create&action=delete";
constexpr char kAppView[] = "https://public.api.bsky.app";
const char* kJets[] = {
    "wss://jetstream2.us-east.bsky.network/subscribe",
    "wss://jetstream1.us-east.bsky.network/subscribe",
    "wss://jetstream2.us-west.bsky.network/subscribe",
};

extern "C" {
typedef struct evp_pkey_st EVP_PKEY;
typedef struct evp_md_st EVP_MD;
typedef struct evp_md_ctx_st EVP_MD_CTX;
typedef struct evp_pkey_ctx_st EVP_PKEY_CTX;
typedef struct engine_st ENGINE;

EVP_PKEY* EVP_PKEY_Q_keygen(void* libctx, const char* propq, const char* type, ...);
int EVP_PKEY_get_octet_string_param(const EVP_PKEY* pkey, const char* key_name, unsigned char* buf, size_t max_buf_sz, size_t* out_len);
int i2d_PrivateKey(const EVP_PKEY* a, unsigned char** pp);
EVP_PKEY* d2i_AutoPrivateKey(EVP_PKEY** a, const unsigned char** pp, long length);
void EVP_PKEY_free(EVP_PKEY* pkey);
void CRYPTO_free(void* ptr, const char* file, int line);
EVP_MD_CTX* EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX* ctx);
const EVP_MD* EVP_sha256(void);
int EVP_DigestSignInit(EVP_MD_CTX* ctx, EVP_PKEY_CTX** pctx, const EVP_MD* type, ENGINE* e, EVP_PKEY* pkey);
int EVP_DigestSign(EVP_MD_CTX* ctx, unsigned char* sigret, size_t* siglen, const unsigned char* tbs, size_t tbslen);
int EVP_DigestVerifyInit(EVP_MD_CTX* ctx, EVP_PKEY_CTX** pctx, const EVP_MD* type, ENGINE* e, EVP_PKEY* pkey);
int EVP_DigestVerify(EVP_MD_CTX* ctx, const unsigned char* sigret, size_t siglen, const unsigned char* tbs, size_t tbslen);
int RAND_bytes(unsigned char* buf, int num);
}

QByteArray b64url(const QByteArray& in) {
    return in.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QByteArray randomBytes(int n) {
    QByteArray out(n, 0);
    RAND_bytes(reinterpret_cast<unsigned char*>(out.data()), n);
    return out;
}

QString randomToken(int n) { return QString::fromLatin1(b64url(randomBytes(n))); }

bool derToP1363(const unsigned char* der, size_t len, unsigned char out[64]) {
    if (len < 8 || der[0] != 0x30 || (der[1] & 0x80)) return false;
    size_t i = 2;
    for (int part = 0; part < 2; ++part) {
        if (i >= len || der[i++] != 0x02) return false;
        if (i >= len) return false;
        int n = der[i++];
        if (n < 1 || i + static_cast<size_t>(n) > len) return false;
        const unsigned char* p = der + i;
        i += n;
        if (p[0] == 0x00 && n > 1) {
            ++p;
            --n;
        }
        if (n > 32) return false;
        unsigned char* dst = out + part * 32;
        std::memset(dst, 0, 32);
        std::memcpy(dst + (32 - n), p, static_cast<size_t>(n));
    }
    return true;
}

bool p1363ToDer(const unsigned char raw[64], QByteArray& der) {
    unsigned char body[80];
    int n = 0;
    for (int part = 0; part < 2; ++part) {
        const unsigned char* src = raw + part * 32;
        int start = 0;
        while (start < 31 && src[start] == 0) ++start;
        int len = 32 - start;
        int pad = (src[start] & 0x80) ? 1 : 0;
        if (n + 2 + pad + len > static_cast<int>(sizeof body)) return false;
        body[n++] = 0x02;
        body[n++] = static_cast<unsigned char>(len + pad);
        if (pad) body[n++] = 0x00;
        std::memcpy(body + n, src + start, static_cast<size_t>(len));
        n += len;
    }
    der.resize(n + 2);
    der[0] = 0x30;
    der[1] = static_cast<char>(n);
    std::memcpy(der.data() + 2, body, static_cast<size_t>(n));
    return true;
}

QString htuOf(const QUrl& url) {
    QUrl u = url;
    u.setQuery(QString());
    u.setFragment(QString());
    return u.toString(QUrl::FullyEncoded | QUrl::RemoveUserInfo);
}

QString pdsOf(const QJsonObject& doc) {
    const QJsonArray services = doc.value("service").toArray();
    for (const QJsonValue& v : services) {
        const QJsonObject s = v.toObject();
        const QString id = s.value("id").toString();
        const QString type = s.value("type").toString();
        if (!id.contains("atproto_pds") && type != "AtprotoPersonalDataServer") continue;
        const QJsonValue ep = s.value("serviceEndpoint");
        if (ep.isString()) return ep.toString();
        if (ep.isObject()) {
            const QString uri = ep.toObject().value("uri").toString();
            if (!uri.isEmpty()) return uri;
        }
    }
    return {};
}

QString originOf(const QString& url) {
    const QUrl u(url);
    if (!u.isValid() || u.host().isEmpty()) return url;
    QString out = u.scheme() + "://" + u.host();
    if (u.port() > 0) out += ":" + QString::number(u.port());
    return out;
}

QString isoNow() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString errText(const QByteArray& body, const QString& fallback) {
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    const QString desc = o.value("error_description").toString();
    if (!desc.isEmpty()) return desc;
    const QString err = o.value("error").toString();
    const QString msg = o.value("message").toString();
    if (!err.isEmpty() && !msg.isEmpty()) return err + ": " + msg;
    if (!msg.isEmpty()) return msg;
    if (!err.isEmpty()) return err;
    return fallback;
}

}  // namespace

struct BskyClient::Key {
    EVP_PKEY* pkey = nullptr;
    QString x;
    QString y;

    ~Key() { clear(); }
    void clear() {
        if (pkey) EVP_PKEY_free(pkey);
        pkey = nullptr;
        x.clear();
        y.clear();
    }
    bool readPub() {
        unsigned char pub[80];
        size_t len = 0;
        if (!pkey || EVP_PKEY_get_octet_string_param(pkey, "pub", pub, sizeof pub, &len) != 1) return false;
        if (len != 65 || pub[0] != 0x04) return false;
        x = QString::fromLatin1(b64url(QByteArray(reinterpret_cast<const char*>(pub + 1), 32)));
        y = QString::fromLatin1(b64url(QByteArray(reinterpret_cast<const char*>(pub + 33), 32)));
        return true;
    }
    bool generate() {
        clear();
        pkey = EVP_PKEY_Q_keygen(nullptr, nullptr, "EC", "P-256");
        return pkey && readPub();
    }
    bool load(const QByteArray& der) {
        clear();
        const unsigned char* p = reinterpret_cast<const unsigned char*>(der.constData());
        pkey = d2i_AutoPrivateKey(nullptr, &p, der.size());
        return pkey && readPub();
    }
    QByteArray store() const {
        if (!pkey) return {};
        unsigned char* der = nullptr;
        const int n = i2d_PrivateKey(pkey, &der);
        if (n <= 0 || !der) return {};
        QByteArray out(reinterpret_cast<const char*>(der), n);
        CRYPTO_free(der, "", 0);
        return out;
    }
    QByteArray sign(const QByteArray& msg) const {
        if (!pkey) return {};
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) return {};
        unsigned char sig[160];
        size_t siglen = sizeof sig;
        const int ok = EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey) == 1 &&
                       EVP_DigestSign(ctx, sig, &siglen, reinterpret_cast<const unsigned char*>(msg.constData()), static_cast<size_t>(msg.size())) == 1;
        EVP_MD_CTX_free(ctx);
        if (!ok) return {};
        unsigned char raw[64];
        if (!derToP1363(sig, siglen, raw)) return {};
        return QByteArray(reinterpret_cast<const char*>(raw), 64);
    }
};

BskyNote parseBskyNote(const QString& postText) {
    BskyNote n;
    QString line = postText;
    const int nl = line.indexOf('\n');
    if (nl >= 0) line = line.left(nl);
    line = line.trimmed();
    if (!line.startsWith("AC1 ")) return n;
    const QStringList p = line.split(' ', Qt::SkipEmptyParts);
    if (p.size() < 2) return n;
    auto nonceOk = [](const QString& s) {
        if (s.size() < 6 || s.size() > 16) return false;
        for (const QChar c : s) {
            if (c.isDigit()) continue;
            if (c < QLatin1Char('a') || c > QLatin1Char('f')) return false;
        }
        return true;
    };
    n.verb = p.at(1);
    if (n.verb == "SIT" && p.size() >= 4 && nonceOk(p.at(2))) {
        n.nonce = p.at(2);
        n.side = p.at(3).toInt();
        n.text = p.mid(4).join(' ');
        n.ok = n.side == 0 || n.side == 1;
    } else if (n.verb == "START" && p.size() >= 4 && nonceOk(p.at(2))) {
        n.nonce = p.at(2);
        n.side = p.at(3).toInt();
        n.ok = n.side == 0 || n.side == 1;
    } else if (n.verb == "MOVE" && p.size() >= 7 && nonceOk(p.at(2))) {
        n.nonce = p.at(2);
        n.ply = p.at(3).toInt();
        n.from = p.at(4).toInt();
        n.to = p.at(5).toInt();
        n.promo = p.at(6).toInt();
        n.ok = n.ply >= 0 && n.from >= 0 && n.from < 64 && n.to >= 0 && n.to < 64 && n.promo >= 0 && n.promo <= 5;
    } else if (n.verb == "ACK" && p.size() >= 4 && nonceOk(p.at(2))) {
        n.nonce = p.at(2);
        n.ply = p.at(3).toInt();
        n.ok = n.ply >= 0;
    } else if ((n.verb == "RESIGN" || n.verb == "REMATCH") && p.size() >= 3 && nonceOk(p.at(2))) {
        n.nonce = p.at(2);
        n.ok = true;
    } else if (n.verb == "DRAW" && p.size() >= 4 && nonceOk(p.at(2))) {
        n.nonce = p.at(2);
        n.text = p.at(3);
        n.ok = n.text == "offer" || n.text == "yes" || n.text == "no";
    } else if (n.verb == "CHAT" && p.size() >= 4 && nonceOk(p.at(2))) {
        n.nonce = p.at(2);
        n.text = p.mid(3).join(' ');
        n.ok = !n.text.isEmpty();
    }
    return n;
}

int bskySelfTest() {
    const BskyNote sit = parseBskyNote("AC1 SIT abcdef01 1 Warchief\nSitting down.");
    if (!sit.ok || sit.verb != "SIT" || sit.side != 1 || sit.text != "Warchief") {
        std::fprintf(stderr, "bsky sit parse failed\n");
        return 1;
    }
    const BskyNote mv = parseBskyNote("AC1 MOVE abcdef01 3 12 28 5\ne7");
    if (!mv.ok || mv.ply != 3 || mv.from != 12 || mv.to != 28 || mv.promo != 5) {
        std::fprintf(stderr, "bsky move parse failed\n");
        return 1;
    }
    if (parseBskyNote("hello").ok || parseBskyNote("AC1 MOVE zz 1 1 1 0").ok) {
        std::fprintf(stderr, "bsky parse accepted junk\n");
        return 1;
    }
    BskyClient::Key key;
    if (!key.generate()) {
        std::fprintf(stderr, "bsky key generate failed\n");
        return 1;
    }
    const QByteArray der = key.store();
    const QByteArray sig = key.sign("ashencrown");
    if (der.isEmpty() || sig.size() != 64) {
        std::fprintf(stderr, "bsky sign failed\n");
        return 1;
    }
    BskyClient::Key again;
    if (!again.load(der)) {
        std::fprintf(stderr, "bsky key reload failed\n");
        return 1;
    }
    QByteArray derSig;
    if (!p1363ToDer(reinterpret_cast<const unsigned char*>(sig.constData()), derSig)) {
        std::fprintf(stderr, "bsky p1363 failed\n");
        return 1;
    }
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    const unsigned char* msg = reinterpret_cast<const unsigned char*>("ashencrown");
    const int ok = ctx && EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, again.pkey) == 1 &&
                   EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char*>(derSig.constData()), static_cast<size_t>(derSig.size()), msg, 10) == 1;
    if (ctx) EVP_MD_CTX_free(ctx);
    if (!ok) {
        std::fprintf(stderr, "bsky verify failed\n");
        return 1;
    }
    std::cout << "bsky ok\n";
    return 0;
}

BskyClient::BskyClient(QObject* parent) : QObject(parent), net_(new QNetworkAccessManager(this)), key_(new Key) {
    reconnect_ = new QTimer(this);
    reconnect_->setSingleShot(true);
    connect(reconnect_, &QTimer::timeout, this, [this]() { openSocket(); });
}

BskyClient::~BskyClient() {
    unwatch();
    delete key_;
}

void BskyClient::setStatus(const QString& text) {
    status_ = text;
    emit changed();
}

void BskyClient::restore() {
    QSettings s;
    const QByteArray der = QByteArray::fromBase64(s.value("bsky/key").toByteArray());
    access_ = s.value("bsky/access").toString();
    refresh_ = s.value("bsky/refresh").toString();
    did_ = s.value("bsky/did").toString();
    handle_ = s.value("bsky/handle").toString();
    pds_ = s.value("bsky/pds").toString();
    issuer_ = s.value("bsky/issuer").toString();
    tokenEndpoint_ = s.value("bsky/token").toString();
    clientId_ = s.value("bsky/client").toString();
    nonce_ = s.value("bsky/nonce").toString();
    expiresAt_ = s.value("bsky/exp").toLongLong();
    if (der.isEmpty() || refresh_.isEmpty() || did_.isEmpty() || !key_->load(der)) {
        clearSession();
        return;
    }
    loggedIn_ = true;
    status_ = "Signed in as @" + handle_ + ".";
}

void BskyClient::saveSession() const {
    QSettings s;
    s.setValue("bsky/key", key_->store().toBase64());
    s.setValue("bsky/access", access_);
    s.setValue("bsky/refresh", refresh_);
    s.setValue("bsky/did", did_);
    s.setValue("bsky/handle", handle_);
    s.setValue("bsky/pds", pds_);
    s.setValue("bsky/issuer", issuer_);
    s.setValue("bsky/token", tokenEndpoint_);
    s.setValue("bsky/client", clientId_);
    s.setValue("bsky/nonce", nonce_);
    s.setValue("bsky/exp", expiresAt_);
}

void BskyClient::clearSession() {
    QSettings s;
    s.remove("bsky/key");
    s.remove("bsky/access");
    s.remove("bsky/refresh");
    s.remove("bsky/did");
    s.remove("bsky/handle");
    s.remove("bsky/pds");
    s.remove("bsky/issuer");
    s.remove("bsky/token");
    s.remove("bsky/client");
    s.remove("bsky/nonce");
    s.remove("bsky/exp");
    loggedIn_ = false;
    access_.clear();
    refresh_.clear();
    did_.clear();
    handle_.clear();
    expiresAt_ = 0;
    key_->clear();
}

void BskyClient::logout() {
    const int gen = ++loginGen_;
    Q_UNUSED(gen);
    busy_ = false;
    if (server_) server_->close();
    unwatch();
    queue_.clear();
    pumping_ = false;
    clearSession();
    setStatus("Signed out of Bluesky.");
}

QString BskyClient::proof(const QString& method, const QString& url, const QString& access) const {
    QJsonObject jwk;
    jwk["crv"] = "P-256";
    jwk["kty"] = "EC";
    jwk["x"] = key_->x;
    jwk["y"] = key_->y;
    QJsonObject header;
    header["alg"] = "ES256";
    header["jwk"] = jwk;
    header["typ"] = "dpop+jwt";
    QJsonObject payload;
    payload["htm"] = method;
    payload["htu"] = url;
    payload["iat"] = static_cast<int>(QDateTime::currentSecsSinceEpoch());
    payload["jti"] = randomToken(12);
    if (!nonce_.isEmpty()) payload["nonce"] = nonce_;
    if (!access.isEmpty())
        payload["ath"] = QString::fromLatin1(b64url(QCryptographicHash::hash(access.toUtf8(), QCryptographicHash::Sha256)));
    const QByteArray signing = b64url(QJsonDocument(header).toJson(QJsonDocument::Compact)) + "." +
                               b64url(QJsonDocument(payload).toJson(QJsonDocument::Compact));
    const QByteArray sig = key_->sign(signing);
    if (sig.isEmpty()) return {};
    return QString::fromLatin1(signing + "." + b64url(sig));
}

void BskyClient::request(const QString& method, const QUrl& url, const QByteArray& body, const QByteArray& contentType, bool authed, bool retry,
                         const std::function<void(int, const QByteArray&, const QByteArray&)>& done) {
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("User-Agent", "AshenCrown");
    req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    if (authed) {
        const QString jwt = proof(method, htuOf(url), access_);
        if (jwt.isEmpty()) {
            done(0, "The signing key failed.", {});
            return;
        }
        req.setRawHeader("Authorization", "DPoP " + access_.toUtf8());
        req.setRawHeader("DPoP", jwt.toUtf8());
    } else if (method == "POST") {
        const QString jwt = proof(method, htuOf(url), QString());
        if (!jwt.isEmpty()) req.setRawHeader("DPoP", jwt.toUtf8());
    }
    QNetworkReply* reply = method == "POST" ? net_->post(req, body) : net_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, method, url, body, contentType, authed, retry, done]() {
        const QByteArray fresh = reply->rawHeader("DPoP-Nonce");
        if (!fresh.isEmpty()) nonce_ = QString::fromUtf8(fresh);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();
        const QString error = QJsonDocument::fromJson(raw).object().value("error").toString();
        reply->deleteLater();
        if (!retry && (error == "use_dpop_nonce" || (status == 401 && error == "InvalidToken" && !fresh.isEmpty() && authed))) {
            request(method, url, body, contentType, authed, true, done);
            return;
        }
        done(status, raw, fresh);
    });
}

void BskyClient::failLogin(const QString& why) {
    busy_ = false;
    if (server_) server_->close();
    if (!priorKey_.isEmpty()) key_->load(priorKey_);
    priorKey_.clear();
    setStatus(why);
}

void BskyClient::login(const QString& handleIn) {
    if (busy_) return;
    QString handle = handleIn.trimmed();
    if (handle.startsWith('@')) handle = handle.mid(1);
    handle = handle.toLower();
    if (handle.isEmpty() || !handle.contains('.')) {
        setStatus("Type your Bluesky handle, such as you.bsky.social.");
        return;
    }
    busy_ = true;
    const int gen = ++loginGen_;
    priorKey_ = key_->store();
    if (!key_->generate()) {
        failLogin("This machine could not make a signing key.");
        return;
    }
    setStatus("Finding your account…");
    beginResolve(handle, [this, gen, handle](bool ok, const QString& did, const QString& resolved, const QString& pds) {
        if (gen != loginGen_) return;
        if (!ok || pds.isEmpty()) {
            failLogin(ok ? "That account has no personal data server." : "Could not find that Bluesky handle.");
            return;
        }
        did_ = did;
        handle_ = resolved.isEmpty() ? handle : resolved;
        pds_ = pds;
        const QUrl res(originOf(pds) + "/.well-known/oauth-protected-resource");
        request("GET", res, {}, "application/json", false, false, [this, gen](int status, const QByteArray& body, const QByteArray&) {
            if (gen != loginGen_) return;
            QString as = originOf(pds_);
            if (status >= 200 && status < 300) {
                const QJsonArray list = QJsonDocument::fromJson(body).object().value("authorization_servers").toArray();
                if (!list.isEmpty()) as = list.at(0).toString();
            }
            const QUrl meta(originOf(as) + "/.well-known/oauth-authorization-server");
            request("GET", meta, {}, "application/json", false, false, [this, gen](int st, const QByteArray& raw, const QByteArray&) {
                if (gen != loginGen_) return;
                const QJsonObject o = QJsonDocument::fromJson(raw).object();
                parEndpoint_ = o.value("pushed_authorization_request_endpoint").toString();
                authEndpoint_ = o.value("authorization_endpoint").toString();
                tokenEndpoint_ = o.value("token_endpoint").toString();
                issuer_ = o.value("issuer").toString();
                if (st < 200 || st >= 300 || parEndpoint_.isEmpty() || authEndpoint_.isEmpty() || tokenEndpoint_.isEmpty()) {
                    failLogin("Bluesky did not offer a sign-in for that server.");
                    return;
                }
                if (!server_) server_ = new QTcpServer(this);
                server_->close();
                QObject::disconnect(server_, &QTcpServer::newConnection, nullptr, nullptr);
                if (!server_->listen(QHostAddress::LocalHost, 0)) {
                    failLogin("Could not open the local sign-in door.");
                    return;
                }
                connect(server_, &QTcpServer::newConnection, this, [this, gen]() {
                    if (gen != loginGen_) return;
                    QTcpSocket* sock = server_->nextPendingConnection();
                    if (!sock) return;
                    auto buf = std::make_shared<QByteArray>();
                    connect(sock, &QTcpSocket::readyRead, this, [this, gen, sock, buf]() {
                        buf->append(sock->readAll());
                        const int end = buf->indexOf("\r\n");
                        if (end < 0) return;
                        const QByteArray line = buf->left(end);
                        buf->clear();
                        const QList<QByteArray> parts = line.split(' ');
                        const QString html = "<html><body style='background:#140e0a;color:#f3e6c8;font-family:serif;text-align:center;padding-top:10%'>"
                                             "<h1>Ashen Crown</h1><p>The sign is in. You can close this window.</p></body></html>";
                        const QByteArray page = html.toUtf8();
                        const QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: " +
                                                 QByteArray::number(page.size()) + "\r\n\r\n" + page;
                        sock->write(resp);
                        sock->disconnectFromHost();
                        sock->deleteLater();
                        if (gen != loginGen_ || parts.size() < 2) return;
                        const QUrl req("http://127.0.0.1" + QString::fromUtf8(parts.at(1)));
                        if (req.path() != "/callback") return;
                        const QUrlQuery q(req);
                        if (q.queryItemValue("state") != state_) return;
                        const QString problem = q.queryItemValue("error");
                        if (!problem.isEmpty()) {
                            const QString desc = QUrl::fromPercentEncoding(q.queryItemValue("error_description").toUtf8());
                            failLogin(desc.isEmpty() ? "Bluesky declined the sign-in." : desc);
                            return;
                        }
                        const QString iss = QUrl::fromPercentEncoding(q.queryItemValue("iss").toUtf8());
                        if (!iss.isEmpty() && originOf(iss) != originOf(issuer_) && iss != issuer_) {
                            failLogin("The sign-in came back from an unexpected server.");
                            return;
                        }
                        const QString code = q.queryItemValue("code");
                        if (code.isEmpty()) return;
                        server_->close();
                        QUrlQuery form;
                        form.addQueryItem("grant_type", "authorization_code");
                        form.addQueryItem("code", code);
                        form.addQueryItem("redirect_uri", redirect_);
                        form.addQueryItem("client_id", clientId_);
                        form.addQueryItem("code_verifier", verifier_);
                        setStatus("Bringing the sign back…");
                        request("POST", QUrl(tokenEndpoint_), form.query(QUrl::FullyEncoded).toUtf8(), "application/x-www-form-urlencoded", false, false,
                                [this, gen](int status, const QByteArray& body, const QByteArray&) {
                                    if (gen != loginGen_) return;
                                    const QJsonObject o = QJsonDocument::fromJson(body).object();
                                    const QString sub = o.value("sub").toString();
                                    const QString granted = o.value("scope").toString();
                                    if (status < 200 || status >= 300 || o.value("access_token").toString().isEmpty() || sub != did_ ||
                                        !granted.split(' ', Qt::SkipEmptyParts).contains(QStringLiteral("atproto"))) {
                                        failLogin(errText(body, "Bluesky did not finish the sign-in."));
                                        return;
                                    }
                                    access_ = o.value("access_token").toString();
                                    refresh_ = o.value("refresh_token").toString();
                                    const int life = o.value("expires_in").toInt(3600);
                                    expiresAt_ = QDateTime::currentSecsSinceEpoch() + life - 30;
                                    loggedIn_ = true;
                                    busy_ = false;
                                    priorKey_.clear();
                                    saveSession();
                                    setStatus("Signed in as @" + handle_ + ".");
                                    pump();
                                });
                    });
                });
                redirect_ = QString("http://127.0.0.1:%1/callback").arg(server_->serverPort());
                verifier_ = randomToken(32);
                state_ = randomToken(16);
                sendPar(gen, QString::fromLatin1(kScope), false);
            });
        });
    });
}

void BskyClient::sendPar(int gen, const QString& scope, bool fallback) {
    if (gen != loginGen_) return;
    clientId_ = QStringLiteral("http://localhost/?redirect_uri=") +
                QString::fromLatin1(QUrl::toPercentEncoding(redirect_)) + QStringLiteral("&scope=") +
                QString::fromLatin1(QUrl::toPercentEncoding(scope));
    const QByteArray challenge = b64url(QCryptographicHash::hash(verifier_.toUtf8(), QCryptographicHash::Sha256));
    QUrlQuery form;
    form.addQueryItem("response_type", "code");
    form.addQueryItem("client_id", clientId_);
    form.addQueryItem("redirect_uri", redirect_);
    form.addQueryItem("scope", scope);
    form.addQueryItem("state", state_);
    form.addQueryItem("code_challenge", QString::fromLatin1(challenge));
    form.addQueryItem("code_challenge_method", "S256");
    form.addQueryItem("login_hint", handle_);
    setStatus(fallback ? "Asking Bluesky again, with a wider permission…" : "Asking Bluesky to open the sign-in…");
    request("POST", QUrl(parEndpoint_), form.query(QUrl::FullyEncoded).toUtf8(), "application/x-www-form-urlencoded", false, false,
            [this, gen, fallback](int status, const QByteArray& body, const QByteArray&) {
                if (gen != loginGen_) return;
                const QJsonObject o = QJsonDocument::fromJson(body).object();
                const QString requestUri = o.value("request_uri").toString();
                if ((status < 200 || status >= 300 || requestUri.isEmpty()) && !fallback && o.value("error").toString() == "invalid_scope") {
                    sendPar(gen, QStringLiteral("atproto transition:generic"), true);
                    return;
                }
                if (status < 200 || status >= 300 || requestUri.isEmpty()) {
                    failLogin(errText(body, "Bluesky refused the sign-in request."));
                    return;
                }
                QUrl auth(authEndpoint_);
                QUrlQuery aq;
                aq.addQueryItem("client_id", clientId_);
                aq.addQueryItem("request_uri", requestUri);
                auth.setQuery(aq);
                setStatus("A browser window is asking you to allow posts for the match.");
                QDesktopServices::openUrl(auth);
                QTimer::singleShot(180000, this, [this, gen]() {
                    if (gen != loginGen_ || !busy_) return;
                    failLogin("The sign-in waited too long.");
                });
            });
}

void BskyClient::beginResolve(const QString& who, const std::function<void(bool, const QString&, const QString&, const QString&)>& done) {
    QUrl url(QString(kAppView) + "/xrpc/com.atproto.identity.resolveIdentity");
    QUrlQuery q;
    q.addQueryItem("identifier", who);
    url.setQuery(q);
    request("GET", url, {}, "application/json", false, false, [done](int status, const QByteArray& body, const QByteArray&) {
        const QJsonObject o = QJsonDocument::fromJson(body).object();
        const QString did = o.value("did").toString();
        const QString handle = o.value("handle").toString();
        QString pds = pdsOf(o.value("didDoc").toObject());
        if (pds.isEmpty()) pds = pdsOf(o);
        done(status >= 200 && status < 300 && did.startsWith("did:"), did, handle, pds);
    });
}

void BskyClient::createPost(const QString& tag, const QString& text) {
    if (!loggedIn_) {
        emit postFailed(tag, "Sign in with Bluesky first.");
        return;
    }
    Job job;
    job.create = true;
    job.tag = tag;
    job.text = text.left(300);
    queue_.append(job);
    pump();
}

void BskyClient::deletePost(const QString& rkey) {
    if (!loggedIn_ || rkey.isEmpty()) return;
    Job job;
    job.create = false;
    job.rkey = rkey;
    queue_.append(job);
    pump();
}

void BskyClient::pump() {
    if (pumping_ || queue_.isEmpty() || !loggedIn_ || busy_) return;
    if (expiresAt_ > 0 && QDateTime::currentSecsSinceEpoch() > expiresAt_) {
        pumping_ = true;
        QUrlQuery form;
        form.addQueryItem("grant_type", "refresh_token");
        form.addQueryItem("refresh_token", refresh_);
        form.addQueryItem("client_id", clientId_);
        request("POST", QUrl(tokenEndpoint_), form.query(QUrl::FullyEncoded).toUtf8(), "application/x-www-form-urlencoded", false, false,
                [this](int status, const QByteArray& body, const QByteArray&) {
                    pumping_ = false;
                    const QJsonObject o = QJsonDocument::fromJson(body).object();
                    if (status < 200 || status >= 300 || o.value("access_token").toString().isEmpty()) {
                        const QString why = errText(body, "The Bluesky sign-in expired.");
                        queue_.clear();
                        clearSession();
                        setStatus(why + " Sign in again.");
                        return;
                    }
                    access_ = o.value("access_token").toString();
                    if (!o.value("refresh_token").toString().isEmpty()) refresh_ = o.value("refresh_token").toString();
                    expiresAt_ = QDateTime::currentSecsSinceEpoch() + o.value("expires_in").toInt(3600) - 30;
                    saveSession();
                    pump();
                });
        return;
    }
    pumping_ = true;
    const Job job = queue_.takeFirst();
    QJsonObject record;
    QJsonObject body;
    body["repo"] = did_;
    body["collection"] = "app.bsky.feed.post";
    if (job.create) {
        record["$type"] = "app.bsky.feed.post";
        record["text"] = job.text;
        record["createdAt"] = isoNow();
        body["record"] = record;
    } else {
        body["rkey"] = job.rkey;
    }
    const QString methodName = job.create ? "com.atproto.repo.createRecord" : "com.atproto.repo.deleteRecord";
    request("POST", QUrl(originOf(pds_) + "/xrpc/" + methodName), QJsonDocument(body).toJson(QJsonDocument::Compact), "application/json", true, false,
            [this, job](int status, const QByteArray& raw, const QByteArray&) {
                pumping_ = false;
                if (status < 200 || status >= 300) {
                    if (job.create) emit postFailed(job.tag, errText(raw, "The post did not go up."));
                    pump();
                    return;
                }
                if (job.create) {
                    const QString uri = QJsonDocument::fromJson(raw).object().value("uri").toString();
                    const QString rkey = uri.section('/', -1);
                    emit postMade(job.tag, rkey);
                }
                if (!nonce_.isEmpty()) saveSession();
                pump();
            });
}

void BskyClient::watch(const QString& handleOrDid) {
    unwatch();
    QString who = handleOrDid.trimmed();
    if (who.startsWith('@')) who = who.mid(1);
    if (!who.startsWith("did:")) who = who.toLower();
    if (who.isEmpty()) {
        setStatus("Type the other commander's Bluesky handle.");
        return;
    }
    watching_ = true;
    const int gen = ++watchGen_;
    jet_ = 0;
    backoffMs_ = 1500;
    setStatus("Looking up @" + who + "…");
    beginResolve(who, [this, gen](bool ok, const QString& did, const QString& handle, const QString&) {
        if (gen != watchGen_ || !watching_) return;
        if (!ok) {
            watching_ = false;
            setStatus("Could not find that Bluesky handle.");
            return;
        }
        watchDid_ = did;
        setStatus("Watching @" + handle + " on the firehose.");
        emit peerReady(did, handle);
        backfill();
        openSocket();
    });
}

void BskyClient::unwatch() {
    watching_ = false;
    ++watchGen_;
    watchDid_.clear();
    if (reconnect_) reconnect_->stop();
    if (socket_) {
        socket_->close();
        socket_->deleteLater();
        socket_ = nullptr;
    }
}

void BskyClient::backfill() {
    if (watchDid_.isEmpty()) return;
    const int gen = watchGen_;
    const QString did = watchDid_;
    QUrl url(QString(kAppView) + "/xrpc/app.bsky.feed.getAuthorFeed");
    QUrlQuery q;
    q.addQueryItem("actor", did);
    q.addQueryItem("limit", "30");
    q.addQueryItem("filter", "posts_no_replies");
    url.setQuery(q);
    request("GET", url, {}, "application/json", false, false, [this, gen, did](int status, const QByteArray& body, const QByteArray&) {
        if (gen != watchGen_ || status < 200 || status >= 300) return;
        struct Row {
            qint64 ms;
            QString rkey;
            QString text;
        };
        QList<Row> rows;
        const QJsonArray feed = QJsonDocument::fromJson(body).object().value("feed").toArray();
        for (const QJsonValue& v : feed) {
            const QJsonObject post = v.toObject().value("post").toObject();
            const QString uri = post.value("uri").toString();
            const QJsonObject rec = post.value("record").toObject();
            const QString text = rec.value("text").toString();
            if (!text.startsWith("AC1 ")) continue;
            const QDateTime when = QDateTime::fromString(rec.value("createdAt").toString(), Qt::ISODateWithMs);
            rows.append({when.isValid() ? when.toMSecsSinceEpoch() : 0, uri.section('/', -1), text});
        }
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.ms < b.ms; });
        for (const Row& row : rows) emit saw(did, "create", row.rkey, row.text, row.ms, false);
    });
}

void BskyClient::armReconnect() {
    if (!watching_) return;
    reconnect_->start(backoffMs_);
    backoffMs_ = std::min(backoffMs_ * 2, 15000);
}

void BskyClient::openSocket() {
    if (!watching_ || watchDid_.isEmpty()) return;
    if (socket_) {
        socket_->close();
        socket_->deleteLater();
        socket_ = nullptr;
    }
    const int gen = watchGen_;
    const QString did = watchDid_;
    QUrl url(kJets[jet_ % 3]);
    QUrlQuery q;
    q.addQueryItem("wantedDids", did);
    q.addQueryItem("wantedCollections", "app.bsky.feed.post");
    url.setQuery(q);
    socket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    connect(socket_, &QWebSocket::connected, this, [this]() { backoffMs_ = 1500; });
    connect(socket_, &QWebSocket::textMessageReceived, this, [this, gen, did](const QString& message) {
        if (gen != watchGen_) return;
        const QJsonObject o = QJsonDocument::fromJson(message.toUtf8()).object();
        if (o.value("kind").toString() != "commit") return;
        const QJsonObject commit = o.value("commit").toObject();
        if (commit.value("collection").toString() != "app.bsky.feed.post") return;
        if (o.value("did").toString() != did) return;
        const QString op = commit.value("operation").toString();
        const QString rkey = commit.value("rkey").toString();
        const QString text = commit.value("record").toObject().value("text").toString();
        const qint64 ms = o.value("time_us").toVariant().toLongLong() / 1000;
        if (op == "create" && !text.startsWith("AC1 ")) return;
        if (op == "create" || op == "delete") emit saw(did, op, rkey, text, ms, true);
    });
    connect(socket_, &QWebSocket::disconnected, this, [this, gen]() {
        if (gen != watchGen_) return;
        jet_ = (jet_ + 1) % 3;
        armReconnect();
    });
    socket_->open(url);
}
