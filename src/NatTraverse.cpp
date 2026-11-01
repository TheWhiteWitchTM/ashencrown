#include "NatTraverse.hpp"

#include <QFile>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <QXmlStreamReader>

NatTraverse::NatTraverse(QObject* parent) : QObject(parent) {
    http_ = new QNetworkAccessManager(this);
    watch_ = new QTimer(this);
    watch_->setSingleShot(true);
    connect(watch_, &QTimer::timeout, this, [this]() {
        if (!busy_) return;
        if (soapStep_ == 0 && pmpStep_ == 0) {
            say("No UPnP gateway answered. Trying NAT-PMP.");
            startNatPmp();
        } else if (pmpStep_ > 0 && !tcpOk_ && !udpOk_) {
            giveUp("The router did not open a port. Same-LAN play still works. Over the internet, forward the port or trade warcodes.");
        }
    });
}

void NatTraverse::say(const QString& line) { emit logLine(line); }

void NatTraverse::begin(const QString& localIp, quint16 tcpPort, quint16 udpPort) {
    clear();
    localIp_ = localIp;
    tcpPort_ = tcpPort;
    udpPort_ = udpPort;
    tcpOk_ = udpOk_ = false;
    externalIp_.clear();
    service_.clear();
    soapStep_ = 0;
    pmpStep_ = 0;
    busy_ = true;
    say("Looking for a UPnP gateway on the local network.");
    sendSearch();
    watch_->start(2500);
}

void NatTraverse::clear() {
    if (mappedTcp_ || mappedUdp_) {
        if (!control_.isEmpty() && !mappedService_.isEmpty()) {
            auto drop = [&](quint16 port, const char* proto) {
                const QString inner = QString("<NewRemoteHost></NewRemoteHost>"
                                              "<NewExternalPort>%1</NewExternalPort>"
                                              "<NewProtocol>%2</NewProtocol>")
                                          .arg(port)
                                          .arg(proto);
                soapStep_ = 9;
                service_ = mappedService_;
                QNetworkRequest req(control_);
                req.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
                req.setRawHeader("SOAPAction", QString("\"%1#DeletePortMapping\"").arg(service_).toUtf8());
                const QString body =
                    "<?xml version=\"1.0\"?>"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                    "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                    "<s:Body><u:DeletePortMapping xmlns:u=\"" +
                    service_ + "\">" + inner + "</u:DeletePortMapping></s:Body></s:Envelope>";
                http_->post(req, body.toUtf8());
            };
            if (mappedTcp_) drop(mappedTcp_, "TCP");
            if (mappedUdp_) drop(mappedUdp_, "UDP");
        }
    }
    mappedTcp_ = mappedUdp_ = 0;
    busy_ = false;
    if (ssdp_) {
        ssdp_->close();
        ssdp_->deleteLater();
        ssdp_ = nullptr;
    }
    if (pmp_) {
        pmp_->close();
        pmp_->deleteLater();
        pmp_ = nullptr;
    }
    watch_->stop();
}

QHostAddress NatTraverse::gateway() const {
    QFile f("/proc/net/route");
    if (!f.open(QIODevice::ReadOnly)) return {};
    f.readLine();
    while (!f.atEnd()) {
        const QString line = QString::fromLatin1(f.readLine());
        const QStringList parts = line.split('\t', Qt::SkipEmptyParts);
        if (parts.size() < 3) continue;
        if (parts[1] != "00000000") continue;
        bool ok = false;
        quint32 raw = parts[2].toUInt(&ok, 16);
        if (!ok || raw == 0) continue;
        quint32 ip = ((raw & 0xFF) << 24) | ((raw & 0xFF00) << 8) | ((raw & 0xFF0000) >> 8) | ((raw >> 24) & 0xFF);
        return QHostAddress(ip);
    }
    return {};
}

void NatTraverse::sendSearch() {
    ssdp_ = new QUdpSocket(this);
    ssdp_->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress);
    connect(ssdp_, &QUdpSocket::readyRead, this, &NatTraverse::onSsdp);
    const QList<QByteArray> searches = {
        "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\n"
        "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n",
        "M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\nMX: 2\r\n"
        "ST: urn:schemas-upnp-org:service:WANIPConnection:1\r\n\r\n"};
    for (const QByteArray& msg : searches)
        ssdp_->writeDatagram(msg, QHostAddress("239.255.255.250"), 1900);
}

void NatTraverse::onSsdp() {
    while (ssdp_ && ssdp_->hasPendingDatagrams()) {
        QByteArray dg;
        dg.resize(int(ssdp_->pendingDatagramSize()));
        ssdp_->readDatagram(dg.data(), dg.size());
        const QString text = QString::fromLatin1(dg);
        QRegularExpression re("(?i)LOCATION:\\s*(\\S+)");
        auto m = re.match(text);
        if (!m.hasMatch()) continue;
        if (soapStep_ != 0) return;
        const QUrl loc(m.captured(1).trimmed());
        say("Gateway answered. Reading its UPnP description.");
        soapStep_ = -1;
        watch_->stop();
        fetchXml(loc);
        return;
    }
}

void NatTraverse::fetchXml(const QUrl& location) {
    QNetworkRequest req(location);
    auto* reply = http_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, location]() {
        const QByteArray body = reply->readAll();
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            say("Could not read the gateway description. Trying NAT-PMP.");
            startNatPmp();
            return;
        }
        QXmlStreamReader xml(body);
        QString type, url, bestType, bestUrl;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) continue;
            const QString n = xml.name().toString();
            if (n == "serviceType") type = xml.readElementText().trimmed();
            else if (n == "controlURL") {
                url = xml.readElementText().trimmed();
                const bool ip = type.contains("WANIPConnection");
                const bool ppp = type.contains("WANPPPConnection");
                if ((ip || ppp) && (bestType.isEmpty() || ip)) {
                    bestType = type;
                    bestUrl = url;
                }
            }
        }
        if (bestType.isEmpty() || bestUrl.isEmpty()) {
            say("This gateway has no WAN IP service. Trying NAT-PMP.");
            startNatPmp();
            return;
        }
        service_ = bestType;
        control_ = location.resolved(QUrl(bestUrl));
        say("Asking the router for the public address.");
        soapStep_ = 1;
        soap("GetExternalIPAddress", "");
    });
}

void NatTraverse::soap(const QString& action, const QString& inner) {
    QNetworkRequest req(control_);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
    req.setRawHeader("SOAPAction", QString("\"%1#%2\"").arg(service_, action).toUtf8());
    const QString body = "<?xml version=\"1.0\"?>"
                         "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                         "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                         "<s:Body><u:" +
                         action + " xmlns:u=\"" + service_ + "\">" + inner + "</u:" + action +
                         "></s:Body></s:Envelope>";
    auto* reply = http_->post(req, body.toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onSoap(reply); });
}

void NatTraverse::onSoap(QNetworkReply* reply) {
    const QByteArray body = reply->readAll();
    const auto err = reply->error();
    reply->deleteLater();
    if (soapStep_ == 9) return;
    const QString text = QString::fromUtf8(body);
    if (text.contains("Fault", Qt::CaseInsensitive) || err != QNetworkReply::NoError) {
        QRegularExpression fault("(?i)<faultstring>([^<]+)</faultstring>");
        auto fm = fault.match(text);
        say(QString("UPnP refused the request%1.")
                .arg(fm.hasMatch() ? ": " + fm.captured(1).trimmed() : QString()));
        if (!tcpOk_ && !udpOk_) startNatPmp();
        else succeed("UPnP");
        return;
    }
    if (soapStep_ == 1) {
        QRegularExpression ipre("(?i)<NewExternalIPAddress>([^<]+)</NewExternalIPAddress>");
        auto m = ipre.match(text);
        if (m.hasMatch()) externalIp_ = m.captured(1).trimmed();
        say(externalIp_.isEmpty() ? "The router hid its public address."
                                  : "Public address is " + externalIp_ + ".");
        soapStep_ = 2;
        say(QString("Asking UPnP to forward TCP %1 to this computer.").arg(tcpPort_));
        const QString inner = QString("<NewRemoteHost></NewRemoteHost>"
                                      "<NewExternalPort>%1</NewExternalPort>"
                                      "<NewProtocol>TCP</NewProtocol>"
                                      "<NewInternalPort>%1</NewInternalPort>"
                                      "<NewInternalClient>%2</NewInternalClient>"
                                      "<NewEnabled>1</NewEnabled>"
                                      "<NewPortMappingDescription>AshenCrown</NewPortMappingDescription>"
                                      "<NewLeaseDuration>7200</NewLeaseDuration>")
                                   .arg(tcpPort_)
                                   .arg(localIp_);
        soap("AddPortMapping", inner);
        return;
    }
    if (soapStep_ == 2) {
        tcpOk_ = true;
        mappedTcp_ = tcpPort_;
        mappedService_ = service_;
        say(QString("TCP %1 is open on the router.").arg(tcpPort_));
        soapStep_ = 3;
        say(QString("Asking UPnP to forward UDP %1.").arg(udpPort_));
        const QString inner = QString("<NewRemoteHost></NewRemoteHost>"
                                      "<NewExternalPort>%1</NewExternalPort>"
                                      "<NewProtocol>UDP</NewProtocol>"
                                      "<NewInternalPort>%1</NewInternalPort>"
                                      "<NewInternalClient>%2</NewInternalClient>"
                                      "<NewEnabled>1</NewEnabled>"
                                      "<NewPortMappingDescription>AshenCrown</NewPortMappingDescription>"
                                      "<NewLeaseDuration>7200</NewLeaseDuration>")
                                   .arg(udpPort_)
                                   .arg(localIp_);
        soap("AddPortMapping", inner);
        return;
    }
    if (soapStep_ == 3) {
        udpOk_ = true;
        mappedUdp_ = udpPort_;
        say(QString("UDP %1 is open on the router.").arg(udpPort_));
        succeed("UPnP");
    }
}

void NatTraverse::startNatPmp() {
    if (pmpStep_ != 0) return;
    soapStep_ = 8;
    const QHostAddress gw = gateway();
    if (gw.isNull()) {
        giveUp("No default gateway, so NAT-PMP has nowhere to go.");
        return;
    }
    pmp_ = new QUdpSocket(this);
    pmp_->bind(QHostAddress::AnyIPv4, 0);
    connect(pmp_, &QUdpSocket::readyRead, this, &NatTraverse::onPmp);
    QByteArray req(2, 0);
    pmpStep_ = 1;
    say("Asking the gateway for NAT-PMP at " + gw.toString() + ".");
    pmp_->writeDatagram(req, gw, 5351);
    watch_->start(1500);
}

void NatTraverse::sendPmpMap(int which) {
    if (!pmp_) return;
    const QHostAddress gw = gateway();
    QByteArray req(12, 0);
    req[0] = 0;
    req[1] = char(which == 0 ? 2 : 1); // 2 TCP, 1 UDP
    quint16 internal = which == 0 ? tcpPort_ : udpPort_;
    quint16 external = internal;
    req[4] = char(internal >> 8);
    req[5] = char(internal);
    req[6] = char(external >> 8);
    req[7] = char(external);
    const quint32 life = 7200;
    req[8] = char(life >> 24);
    req[9] = char(life >> 16);
    req[10] = char(life >> 8);
    req[11] = char(life);
    pmpStep_ = which == 0 ? 2 : 3;
    pmp_->writeDatagram(req, gw, 5351);
}

void NatTraverse::onPmp() {
    while (pmp_ && pmp_->hasPendingDatagrams()) {
        QByteArray dg;
        dg.resize(int(pmp_->pendingDatagramSize()));
        pmp_->readDatagram(dg.data(), dg.size());
        if (dg.size() < 8) continue;
        const auto* p = reinterpret_cast<const uchar*>(dg.constData());
        const int opcode = p[1];
        const int result = (p[2] << 8) | p[3];
        if (opcode == 128 && pmpStep_ == 1 && dg.size() >= 12 && result == 0) {
            externalIp_ = QString("%1.%2.%3.%4").arg(p[8]).arg(p[9]).arg(p[10]).arg(p[11]);
            say("NAT-PMP public address is " + externalIp_ + ".");
            say(QString("Requesting a NAT-PMP map for TCP %1.").arg(tcpPort_));
            sendPmpMap(0);
            watch_->start(1500);
            return;
        }
        if ((opcode == 130 || opcode == 129) && result == 0 && dg.size() >= 16) {
            quint16 ext = quint16((p[10] << 8) | p[11]);
            if (opcode == 130) {
                tcpOk_ = true;
                tcpPort_ = ext;
                mappedTcp_ = ext;
                say(QString("NAT-PMP opened TCP %1.").arg(ext));
                say(QString("Requesting a NAT-PMP map for UDP %1.").arg(udpPort_));
                sendPmpMap(1);
                watch_->start(1500);
            } else {
                udpOk_ = true;
                udpPort_ = ext;
                mappedUdp_ = ext;
                say(QString("NAT-PMP opened UDP %1.").arg(ext));
                succeed("NAT-PMP");
            }
            return;
        }
        if (result != 0) {
            say(QString("NAT-PMP refused the map (code %1).").arg(result));
            if (!tcpOk_ && !udpOk_) giveUp("NAT-PMP failed.");
            else succeed("NAT-PMP");
            return;
        }
    }
}

void NatTraverse::succeed(const QString& method) {
    if (!busy_) return;
    busy_ = false;
    watch_->stop();
    say(method + " is ready. A guest can use the public address, or the warcode.");
    emit finished(tcpOk_, udpOk_, externalIp_, tcpPort_, udpPort_, method);
}

void NatTraverse::giveUp(const QString& why) {
    if (!busy_) return;
    busy_ = false;
    watch_->stop();
    say(why);
    emit finished(tcpOk_, udpOk_, externalIp_, tcpOk_ ? tcpPort_ : 0, udpOk_ ? udpPort_ : 0, "none");
}
