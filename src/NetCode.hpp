#pragma once

#include <QHostAddress>
#include <QString>
#include <QtGlobal>

struct WarCard {
    quint8 flags = 0; // bit 0 tcp, bit 1 udp, bit 2 lan
    quint8 faction = 0;
    quint16 token = 0;
    QHostAddress pub;
    quint16 pubTcp = 0;
    quint16 pubUdp = 0;
    QHostAddress lan;
    quint16 lanPort = 0;
};

QString encodeWarcard(const WarCard& card);
bool decodeWarcard(const QString& text, WarCard& card);
int netCodeSelfTest();
