#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QtGlobal>

class QNetworkAccessManager;
class QNetworkReply;
class QUdpSocket;
class QTimer;

// Opens the host's game ports on a home router.
// UPnP IGD is tried first, then NAT-PMP. The result is a public address a guest can dial.
class NatTraverse : public QObject {
    Q_OBJECT
public:
    explicit NatTraverse(QObject* parent = nullptr);
    void begin(const QString& localIp, quint16 tcpPort, quint16 udpPort);
    void clear();

signals:
    void logLine(const QString& line);
    void finished(bool tcpOk, bool udpOk, const QString& ip, int tcpPort, int udpPort, const QString& method);

private:
    void say(const QString& line);
    void sendSearch();
    void onSsdp();
    void fetchXml(const QUrl& location);
    void soap(const QString& action, const QString& inner);
    void onSoap(QNetworkReply* reply);
    void startNatPmp();
    void onPmp();
    void sendPmpMap(int which);
    void succeed(const QString& method);
    void giveUp(const QString& why);
    QHostAddress gateway() const;

    QString localIp_;
    quint16 tcpPort_ = 0;
    quint16 udpPort_ = 0;
    QString service_;
    QUrl control_;
    QString externalIp_;
    bool tcpOk_ = false;
    bool udpOk_ = false;
    bool busy_ = false;
    int soapStep_ = 0; // 1 get ip, 2 map tcp, 3 map udp
    int pmpStep_ = 0;  // 1 addr, 2 tcp, 3 udp

    QNetworkAccessManager* http_ = nullptr;
    QUdpSocket* ssdp_ = nullptr;
    QUdpSocket* pmp_ = nullptr;
    QTimer* watch_ = nullptr;
    QString mappedService_;
    quint16 mappedTcp_ = 0;
    quint16 mappedUdp_ = 0;
};
