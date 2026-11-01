#include "BskyClient.hpp"
#include "ChessAI.hpp"
#include "ChessEngine.hpp"
#include "GameController.hpp"
#include "NetCode.hpp"
#include "NetSession.hpp"
#include "version.h"

#include <QCoreApplication>
#ifdef ASHENCROWN_HAS_WEBENGINE
#include <QtWebEngineQuick/qtwebenginequickglobal.h>
#endif
#include <QEventLoop>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

#include <atomic>
#include <cstring>
#include <iostream>

namespace {

int aiSelfTest() {
    ChessEngine engine;
    const std::string before = engine.fen();
    ChessAI ai;
    std::atomic<bool> stop{false};
    const Move m = ai.think(engine, 2, 250, &stop);
    if (engine.fen() != before) {
        std::cerr << "ai mutated the board\n";
        return 1;
    }
    bool legal = false;
    for (const Move& x : engine.legalMoves()) {
        if (x == m) legal = true;
    }
    if (!legal) {
        std::cerr << "ai move is illegal\n";
        return 1;
    }
    std::cout << "ai ok " << engine.san(m) << "\n";
    return 0;
}

int loopbackSelfTest() {
    NetSession host;
    NetSession guest;
    host.start("Alpha", 0, 1001);
    guest.start("Beta", 1, 1002);
    if (host.tcpPort() == 0 || guest.tcpPort() == 0) {
        std::cerr << "loopback bind failed\n";
        return 1;
    }
    QEventLoop loop;
    bool hostUp = false;
    bool guestUp = false;
    QObject::connect(&host, &NetSession::linkUp, &loop, [&](int) {
        hostUp = true;
        if (guestUp) loop.quit();
    });
    QObject::connect(&guest, &NetSession::linkUp, &loop, [&](int) {
        guestUp = true;
        if (hostUp) loop.quit();
    });
    QTimer::singleShot(8000, &loop, &QEventLoop::quit);
    guest.connectToHost("127.0.0.1", host.tcpPort());
    loop.exec();
    if (!hostUp || !guestUp) {
        std::cerr << "loopback link failed\n";
        host.stop();
        guest.stop();
        return 1;
    }
    QEventLoop waitMsg;
    bool got = false;
    QObject::connect(&host, &NetSession::messageReceived, &waitMsg, [&](const QJsonObject& msg) {
        got = msg.value("t").toString() == "ping" && msg.value("n").toInt() == 7;
        waitMsg.quit();
    });
    QTimer::singleShot(2000, &waitMsg, &QEventLoop::quit);
    QJsonObject ping;
    ping["t"] = "ping";
    ping["n"] = 7;
    guest.send(ping);
    waitMsg.exec();
    host.stop();
    guest.stop();
    if (!got) {
        std::cerr << "loopback message failed\n";
        return 1;
    }
    std::cout << "loopback ok\n";
    return 0;
}

int runSelfTest() {
    if (ChessEngine::chessSelfTest() != 0) return 1;
    if (netCodeSelfTest() != 0) return 1;
    if (aiSelfTest() != 0) return 1;
    if (loopbackSelfTest() != 0) return 1;
    if (bskySelfTest() != 0) return 1;
    std::cout << "ashencrown self-test passed\n";
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    bool self = false;
    bool check = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--selftest") == 0) self = true;
        if (std::strcmp(argv[i], "--check") == 0) check = true;
    }
    if (self) {
        QCoreApplication app(argc, argv);
        return runSelfTest();
    }

    if (check) qputenv("ASHENCROWN_NO_AUDIO", "1");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#ifdef ASHENCROWN_HAS_WEBENGINE
    QtWebEngineQuick::initialize();
#endif
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("AshenCrown");
    QCoreApplication::setApplicationName("AshenCrown");
    QCoreApplication::setApplicationVersion(QStringLiteral(ASHENCROWN_VERSION_STRING));

    GameController game;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Game", &game);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    if (auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()))
        win->setIcon(QIcon(QStringLiteral(":/assets/ui/icon.png")));
    if (check) QTimer::singleShot(0, &app, &QCoreApplication::quit);
    return app.exec();
}
