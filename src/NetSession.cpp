#include "NetSession.hpp"

#include <QDataStream>
#include <QHostAddress>
#include <QHostInfo>
#include <QJsonDocument>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

namespace {

QByteArray packet(quint8 type, quint32 seq, quint32 ack, const QByteArray& body) {
    QByteArray out;
    QDataStream ds(&out, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds.writeRawData("AC", 2);
    ds << type << quint8(0) << seq << ack << quint16(body.size());
    out.append(body);
    return out;
}

bool parsePacket(const QByteArray& dg, quint8& type, quint32& seq, quint32& ack, QByteArray& body) {
    if (dg.size() < 14 || dg[0] != 'A' || dg[1] != 'C') return false;
    QDataStream ds(dg);
    ds.setByteOrder(QDataStream::BigEndian);
    char magic[2];
    ds.readRawData(magic, 2);
    quint8 reserved = 0;
    quint16 len = 0;
    ds >> type >> reserved >> seq >> ack >> len;
    if (ds.status() != QDataStream::Ok) return false;
    if (14 + len > dg.size()) return false;
    body = dg.mid(14, len);
    return true;
}

QString detectLocalIp() {
    QUdpSocket probe;
    probe.connectToHost(QHostAddress("8.8.8.8"), 53);
    probe.waitForConnected(300);
    const QHostAddress addr = probe.localAddress();
    if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback()) return addr.toString();
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning)) continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol && !e.ip().isLoopback()) return e.ip().toString();
        }
    }
    return "127.0.0.1";
}

}  // namespace

NetSession::NetSession(QObject* parent) : QObject(parent) {
    beaconTimer_ = new QTimer(this);
    beaconTimer_->setInterval(1000);
    connect(beaconTimer_, &QTimer::timeout, this, &NetSession::beaconTick);
    udpTimer_ = new QTimer(this);
    udpTimer_->setInterval(200);
    connect(udpTimer_, &QTimer::timeout, this, &NetSession::resendUdp);
    punchTimer_ = new QTimer(this);
    punchTimer_->setInterval(200);
    connect(punchTimer_, &QTimer::timeout, this, [this]() {
        if (!punching_ || linked_ || udpPeerPort_ == 0) return;
        sendUdp(1, 0, {});
    });
}

void NetSession::start(const QString& name, int faction, quint16 token) {
    stop();
    name_ = name;
    faction_ = faction;
    token_ = token;
    localIp_ = detectLocalIp();
    bindTcp();
    bindUdp();
    beacon_ = new QUdpSocket(this);
    if (!beacon_->bind(QHostAddress::AnyIPv4, 41778, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit logLine("LAN beacon could not bind. Direct addresses still work.");
    } else {
        connect(beacon_, &QUdpSocket::readyRead, this, &NetSession::onBeacon);
    }
    beaconTimer_->start();
    emit logLine(QString("Listening on %1, TCP %2, UDP %3.").arg(localIp_).arg(tcpPort_).arg(udpPort_));
    startStun();
}

void NetSession::stop() {
    punchTimer_->stop();
    udpTimer_->stop();
    beaconTimer_->stop();
    punching_ = false;
    if (linked_) drop("Stopped.");
    linked_ = false;
    if (sock_) {
        sock_->disconnect(this);
        sock_->abort();
        sock_->deleteLater();
        sock_ = nullptr;
    }
    if (server_) {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
    }
    if (udp_) {
        udp_->close();
        udp_->deleteLater();
        udp_ = nullptr;
    }
    if (beacon_) {
        beacon_->close();
        beacon_->deleteLater();
        beacon_ = nullptr;
    }
    unacked_.clear();
    tcpBuf_.clear();
    tcpPort_ = udpPort_ = 0;
}

void NetSession::bindTcp() {
    server_ = new QTcpServer(this);
    quint16 port = 41777;
    for (int i = 0; i < 30; ++i) {
        if (server_->listen(QHostAddress::AnyIPv4, port)) break;
        ++port;
    }
    tcpPort_ = server_->serverPort();
    connect(server_, &QTcpServer::newConnection, this, [this]() {
        while (server_->hasPendingConnections()) {
            QTcpSocket* next = server_->nextPendingConnection();
            if (linked_ || sock_) {
                next->disconnectFromHost();
                next->deleteLater();
                continue;
            }
            attachSocket(next, true);
        }
    });
}

void NetSession::bindUdp() {
    udp_ = new QUdpSocket(this);
    quint16 port = 41779;
    for (int i = 0; i < 30; ++i) {
        if (udp_->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) break;
        ++port;
    }
    udpPort_ = udp_->localPort();
    connect(udp_, &QUdpSocket::readyRead, this, &NetSession::pumpUdp);
}

void NetSession::connectToHost(const QString& host, quint16 port) {
    abortConnect();
    sock_ = new QTcpSocket(this);
    connect(sock_, &QTcpSocket::connected, this, [this]() { finishLink(false, "TCP"); });
    connect(sock_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (linked_ || !sock_) return;
        const QString why = sock_->errorString();
        sock_->deleteLater();
        sock_ = nullptr;
        emit connectFailed(why);
    });
    sock_->connectToHost(host, port);
}

void NetSession::abortConnect() {
    punchTimer_->stop();
    punching_ = false;
    if (linked_) return;
    if (sock_) {
        sock_->abort();
        sock_->deleteLater();
        sock_ = nullptr;
    }
}

void NetSession::punchToward(const QString& host, quint16 port) {
    QHostAddress addr(host);
    if (addr.isNull()) {
        const QHostInfo info = QHostInfo::fromName(host);
        if (info.addresses().isEmpty()) {
            emit connectFailed("Could not resolve " + host);
            return;
        }
        addr = info.addresses().first();
    }
    udpPeer_ = addr;
    udpPeerPort_ = port;
    punching_ = true;
    nextSeq_ = 1;
    expectSeq_ = 1;
    peerAck_ = 0;
    unacked_.clear();
    punchTimer_->start();
    sendUdp(1, 0, {});
    emit logLine(QString("Sending UDP punches to %1:%2.").arg(addr.toString()).arg(port));
}

void NetSession::attachSocket(QTcpSocket* sock, bool asHost) {
    sock_ = sock;
    tcpBuf_.clear();
    connect(sock_, &QTcpSocket::readyRead, this, &NetSession::onTcpRead);
    connect(sock_, &QTcpSocket::disconnected, this, [this]() { drop("The peer hung up."); });
    finishLink(asHost, "TCP");
}

void NetSession::finishLink(bool asHost, const QString& how) {
    if (linked_) return;
    linked_ = true;
    punching_ = false;
    punchTimer_->stop();
    emit logLine("Link is up over " + how + ".");
    const int roleHint = asHost ? 1 : (how == "TCP" ? 0 : -1);
    emit linkUp(roleHint);
}

void NetSession::drop(const QString& why) {
    if (!linked_ && !sock_) return;
    linked_ = false;
    punching_ = false;
    punchTimer_->stop();
    udpTimer_->stop();
    unacked_.clear();
    if (sock_) {
        sock_->disconnect(this);
        sock_->deleteLater();
        sock_ = nullptr;
    }
    tcpBuf_.clear();
    emit linkDown(why);
}

void NetSession::send(const QJsonObject& msg) {
    const QByteArray body = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    if (sock_ && sock_->state() == QAbstractSocket::ConnectedState) {
        sock_->write(body);
        sock_->write("\n");
        return;
    }
    if (udpPeerPort_ == 0) return;
    const quint32 seq = nextSeq_++;
    unacked_.insert(seq, body);
    sendUdp(2, seq, body);
    if (!udpTimer_->isActive()) udpTimer_->start();
}

void NetSession::sendUdp(quint8 type, quint32 seq, const QByteArray& body) {
    if (!udp_ || udpPeerPort_ == 0) return;
    udp_->writeDatagram(packet(type, seq, expectSeq_ - (expectSeq_ ? 1 : 0), body), udpPeer_, udpPeerPort_);
}

void NetSession::resendUdp() {
    if (unacked_.isEmpty()) {
        udpTimer_->stop();
        return;
    }
    for (auto it = unacked_.cbegin(); it != unacked_.cend(); ++it) {
        if (it.key() <= peerAck_) continue;
        sendUdp(2, it.key(), it.value());
    }
}

void NetSession::onTcpRead() {
    if (!sock_) return;
    tcpBuf_ += sock_->readAll();
    while (true) {
        int nl = tcpBuf_.indexOf('\n');
        if (nl < 0) break;
        const QByteArray line = tcpBuf_.left(nl);
        tcpBuf_.remove(0, nl + 1);
        deliver(line);
    }
}

void NetSession::deliver(const QByteArray& payload) {
    const auto doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) return;
    emit messageReceived(doc.object());
}

void NetSession::pumpUdp() {
    if (!udp_) return;
    while (udp_->hasPendingDatagrams()) {
        const QNetworkDatagram dg = udp_->receiveDatagram();
        const QByteArray bytes = dg.data();
        if (bytes.size() >= 20 && quint8(bytes[0]) == 0x01 && quint8(bytes[1]) == 0x01) {
            // STUN binding success. Magic cookie at offset 4 must match.
            if (quint8(bytes[4]) != 0x21 || quint8(bytes[5]) != 0x12) continue;
            int off = 20;
            while (off + 4 <= bytes.size()) {
                const int atype = (quint8(bytes[off]) << 8) | quint8(bytes[off + 1]);
                const int alen = (quint8(bytes[off + 2]) << 8) | quint8(bytes[off + 3]);
                if (off + 4 + alen > bytes.size()) break;
                if (atype == 0x0020 && alen >= 8 && bytes[off + 5] == 0x01) {
                    const quint16 xport = quint16((quint8(bytes[off + 6]) << 8) | quint8(bytes[off + 7]));
                    const quint32 xip = (quint32(quint8(bytes[off + 8])) << 24) | (quint32(quint8(bytes[off + 9])) << 16) |
                                        (quint32(quint8(bytes[off + 10])) << 8) | quint32(quint8(bytes[off + 11]));
                    const quint16 port = xport ^ 0x2112;
                    const quint32 ip = xip ^ 0x2112A442;
                    const QString ipText = QHostAddress(ip).toString();
                    emit stunReady(true, ipText, port);
                }
                off += 4 + ((alen + 3) & ~3);
            }
            continue;
        }
        quint8 type = 0;
        quint32 seq = 0, ack = 0;
        QByteArray body;
        if (!parsePacket(bytes, type, seq, ack, body)) continue;
        if (!linked_) {
            udpPeer_ = dg.senderAddress();
            udpPeerPort_ = quint16(dg.senderPort());
            finishLink(punching_ ? false : true, "UDP");
        } else if (dg.senderAddress() == udpPeer_ && dg.senderPort() == udpPeerPort_) {
            // same peer
        } else if (udpPeerPort_ == 0) {
            udpPeer_ = dg.senderAddress();
            udpPeerPort_ = quint16(dg.senderPort());
        }
        if (ack > peerAck_) peerAck_ = ack;
        QList<quint32> done;
        for (auto it = unacked_.cbegin(); it != unacked_.cend(); ++it)
            if (it.key() <= peerAck_) done << it.key();
        for (quint32 s : done) unacked_.remove(s);
        if (type == 2 && !body.isEmpty()) {
            if (seq == expectSeq_) {
                ++expectSeq_;
                deliver(body);
                sendUdp(3, 0, {});
            } else if (seq > expectSeq_) {
                // Drop gaps; chess messages are resent in order by the sender.
                sendUdp(3, 0, {});
            } else {
                sendUdp(3, 0, {});
            }
        }
    }
}

void NetSession::beaconTick() {
    if (!beacon_ || tcpPort_ == 0) return;
    const QByteArray payload =
        QString("ASHENCROWN|1|%1|%2|%3|%4").arg(name_, QString::number(tcpPort_), QString::number(faction_), QString::number(token_)).toUtf8();
    beacon_->writeDatagram(payload, QHostAddress::Broadcast, 41778);
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : ifaces) {
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.broadcast().protocol() == QAbstractSocket::IPv4Protocol)
                beacon_->writeDatagram(payload, e.broadcast(), 41778);
        }
    }
}

void NetSession::onBeacon() {
    while (beacon_ && beacon_->hasPendingDatagrams()) {
        QHostAddress from;
        quint16 port = 0;
        QByteArray dg;
        dg.resize(int(beacon_->pendingDatagramSize()));
        beacon_->readDatagram(dg.data(), dg.size(), &from, &port);
        const QStringList parts = QString::fromUtf8(dg).split('|');
        if (parts.size() < 6 || parts[0] != "ASHENCROWN") continue;
        bool okPort = false, okFac = false, okTok = false;
        const int gamePort = parts[3].toInt(&okPort);
        const int fac = parts[4].toInt(&okFac);
        const int tok = parts[5].toInt(&okTok);
        if (!okPort || !okFac || !okTok || tok == int(token_)) continue;
        emit lanFound(parts[2], from.toString(), gamePort, fac, tok);
    }
}

void NetSession::startStun() {
    if (!udp_) return;
    QHostInfo::lookupHost("stun.l.google.com", this, [this](const QHostInfo& info) {
        if (!udp_ || info.addresses().isEmpty()) {
            emit stunReady(false, {}, 0);
            return;
        }
        QByteArray req(20, 0);
        req[0] = 0x00;
        req[1] = 0x01;
        req[4] = 0x21;
        req[5] = 0x12;
        req[6] = char(0xA4);
        req[7] = 0x42;
        for (int i = 0; i < 12; ++i) req[8 + i] = char(QRandomGenerator::global()->bounded(256));
        stunId_ = req.mid(8, 12);
        udp_->writeDatagram(req, info.addresses().first(), 19302);
        emit logLine("Asking a public STUN server how the internet sees our UDP port.");
    });
}
