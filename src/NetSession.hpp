#pragma once

#include <QHash>
#include <QHostAddress>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QtGlobal>

class QTcpServer;
class QTcpSocket;
class QTimer;
class QUdpSocket;

// TCP for an open or forwarded port, reliable UDP for hole punching,
// and a LAN beacon so two machines on one network find each other.
class NetSession : public QObject {
    Q_OBJECT
public:
    explicit NetSession(QObject* parent = nullptr);

    void start(const QString& name, int faction, quint16 token);
    void stop();
    void connectToHost(const QString& host, quint16 port);
    void abortConnect();
    void punchToward(const QString& host, quint16 port);
    void send(const QJsonObject& msg);

    quint16 tcpPort() const { return tcpPort_; }
    quint16 udpPort() const { return udpPort_; }
    QString localIp() const { return localIp_; }
    bool linked() const { return linked_; }

signals:
    void logLine(const QString& line);
    // 1 locked host (TCP acceptor or passive UDP), 0 locked guest (TCP connector),
    // -1 negotiate (both sides punched; lower hello token hosts).
    void linkUp(int roleHint);
    void linkDown(const QString& why);
    void connectFailed(const QString& why);
    void messageReceived(const QJsonObject& msg);
    void lanFound(const QString& name, const QString& ip, int port, int faction, int token);
    void stunReady(bool ok, const QString& ip, int port);

private:
    void bindTcp();
    void bindUdp();
    void attachSocket(QTcpSocket* sock, bool asHost);
    void onTcpRead();
    void pumpUdp();
    void sendUdp(quint8 type, quint32 seq, const QByteArray& body);
    void resendUdp();
    void deliver(const QByteArray& payload);
    void beaconTick();
    void onBeacon();
    void startStun();
    void finishLink(bool asHost, const QString& how);
    void drop(const QString& why);

    QString name_;
    int faction_ = 0;
    quint16 token_ = 0;
    quint16 tcpPort_ = 0;
    quint16 udpPort_ = 0;
    QString localIp_;
    bool linked_ = false;
    bool punching_ = false;
    quint32 nextSeq_ = 1;
    quint32 expectSeq_ = 1;
    quint32 peerAck_ = 0;

    QTcpServer* server_ = nullptr;
    QTcpSocket* sock_ = nullptr;
    QByteArray tcpBuf_;
    QUdpSocket* udp_ = nullptr;
    QUdpSocket* beacon_ = nullptr;
    QTimer* beaconTimer_ = nullptr;
    QTimer* udpTimer_ = nullptr;
    QTimer* punchTimer_ = nullptr;
    QHostAddress udpPeer_;
    quint16 udpPeerPort_ = 0;
    QHash<quint32, QByteArray> unacked_;
    QByteArray stunId_;
};
