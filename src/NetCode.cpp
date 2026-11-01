#include "NetCode.hpp"

#include <QIODevice>
#include <cstring>
#include <iostream>

namespace {

const char* kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

int alphabetValue(QChar ch) {
    char c = ch.toUpper().toLatin1();
    if (c == 'I' || c == 'L') c = '1';
    if (c == 'O') c = '0';
    const char* p = std::strchr(kAlphabet, c);
    return p ? int(p - kAlphabet) : -1;
}

QString toCrockford(const QByteArray& in) {
    QString out;
    int buffer = 0;
    int bits = 0;
    for (unsigned char c : in) {
        buffer = (buffer << 8) | c;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out += QLatin1Char(kAlphabet[(buffer >> bits) & 31]);
        }
    }
    if (bits) out += QLatin1Char(kAlphabet[(buffer << (5 - bits)) & 31]);
    QString grouped = "AC";
    for (int i = 0; i < out.size(); ++i) {
        if (i % 4 == 0) grouped += '-';
        grouped += out[i];
    }
    return grouped;
}

QByteArray fromCrockford(const QString& text) {
    QByteArray out;
    int buffer = 0;
    int bits = 0;
    for (QChar ch : text) {
        if (ch == '-' || ch.isSpace() || ch == '_') continue;
        if (text.startsWith("AC") && out.isEmpty() && bits == 0 && (ch == 'A' || ch == 'a')) {
            // handled below by skipping a leading AC tag outside this loop
        }
        int v = alphabetValue(ch);
        if (v < 0) continue;
        buffer = (buffer << 5) | v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.append(char((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

void put16(uchar* p, int o, quint16 v) {
    p[o] = uchar(v >> 8);
    p[o + 1] = uchar(v);
}

quint16 get16(const uchar* p, int o) { return quint16((p[o] << 8) | p[o + 1]); }

void put32(uchar* p, int o, quint32 v) {
    p[o] = uchar(v >> 24);
    p[o + 1] = uchar(v >> 16);
    p[o + 2] = uchar(v >> 8);
    p[o + 3] = uchar(v);
}

quint32 get32(const uchar* p, int o) {
    return (quint32(p[o]) << 24) | (quint32(p[o + 1]) << 16) | (quint32(p[o + 2]) << 8) | quint32(p[o + 3]);
}

}  // namespace

QString encodeWarcard(const WarCard& card) {
    QByteArray raw(20, 0);
    auto* p = reinterpret_cast<uchar*>(raw.data());
    p[0] = 1;
    p[1] = card.flags;
    p[2] = card.faction;
    put32(p, 4, card.pub.protocol() == QAbstractSocket::IPv4Protocol ? card.pub.toIPv4Address() : 0);
    put16(p, 8, card.pubTcp);
    put16(p, 10, card.pubUdp);
    put32(p, 12, card.lan.protocol() == QAbstractSocket::IPv4Protocol ? card.lan.toIPv4Address() : 0);
    put16(p, 16, card.lanPort);
    put16(p, 18, card.token);
    return toCrockford(raw);
}

bool decodeWarcard(const QString& text, WarCard& card) {
    QString s = text.trimmed();
    if (s.startsWith("AC-", Qt::CaseInsensitive) || s.startsWith("AC", Qt::CaseInsensitive)) {
        if (s.size() >= 2 && s.left(2).compare("AC", Qt::CaseInsensitive) == 0) {
            s = s.mid(2);
            if (s.startsWith('-')) s = s.mid(1);
        }
    } else {
        return false;
    }
    QByteArray raw = fromCrockford(s);
    if (raw.size() < 20) return false;
    const auto* p = reinterpret_cast<const uchar*>(raw.constData());
    if (p[0] != 1) return false;
    card.flags = p[1];
    card.faction = p[2];
    quint32 pub = get32(p, 4);
    card.pub = pub ? QHostAddress(pub) : QHostAddress();
    card.pubTcp = get16(p, 8);
    card.pubUdp = get16(p, 10);
    quint32 lan = get32(p, 12);
    card.lan = lan ? QHostAddress(lan) : QHostAddress();
    card.lanPort = get16(p, 16);
    card.token = get16(p, 18);
    return true;
}

int netCodeSelfTest() {
    WarCard in;
    in.flags = 1 | 2 | 4;
    in.faction = 1;
    in.token = 0xBEEF;
    in.pub = QHostAddress("203.0.113.10");
    in.pubTcp = 41777;
    in.pubUdp = 41779;
    in.lan = QHostAddress("192.168.1.40");
    in.lanPort = 41777;
    const QString code = encodeWarcard(in);
    WarCard out;
    if (!decodeWarcard(code, out)) {
        std::cerr << "warcode failed to decode: " << code.toStdString() << "\n";
        return 1;
    }
    if (out.flags != in.flags || out.faction != in.faction || out.token != in.token || out.pubTcp != in.pubTcp ||
        out.pubUdp != in.pubUdp || out.lanPort != in.lanPort || out.pub != in.pub || out.lan != in.lan) {
        std::cerr << "warcode mismatch\n";
        return 1;
    }
    QString messy = code;
    messy.replace("-", " - ");
    WarCard again;
    if (!decodeWarcard(messy.toLower(), again) || again.token != in.token) {
        std::cerr << "warcode lowercase failed\n";
        return 1;
    }
    std::cout << "warcode ok " << code.toStdString() << "\n";
    return 0;
}
