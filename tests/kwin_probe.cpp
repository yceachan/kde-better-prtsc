#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusVariant>

#include <cstdio>
#include <cstring>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (std::strcmp(qVersion(), "6.2.4") != 0) {
        std::fprintf(stderr, "Qt 6.2.4 required\n");
        return 1;
    }
    const auto bus = QDBusConnection::sessionBus();
    const auto message = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.KWin.ScreenShot2"), QStringLiteral("/org/kde/KWin/ScreenShot2"),
        QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    auto request = message;
    request.setArguments({QStringLiteral("org.kde.KWin.ScreenShot2"), QStringLiteral("Version")});
    const auto synchronous = bus.call(request, QDBus::Block, 3000);
    auto pending = bus.asyncCall(request, 3000);
    pending.waitForFinished();
    for (const auto &reply : {synchronous, pending.reply()}) {
        if (reply.type() == QDBusMessage::ErrorMessage) {
            std::fprintf(stderr, "%s: %s\n", qPrintable(reply.errorName()), qPrintable(reply.errorMessage()));
            return 1;
        }
        if (reply.arguments().size() != 1) {
            std::fprintf(stderr, "Unexpected KWin property reply\n");
            return 1;
        }
        const auto version = reply.arguments()[0].value<QDBusVariant>().variant().toUInt();
        if (version < 5) {
            std::fprintf(stderr, "KWin ScreenShot2 Version >= 5 required, found %u\n", version);
            return 1;
        }
        std::printf("PASS: real Qt %s -> KWin ScreenShot2 Version %u\n", qVersion(), version);
    }
    return 0;
}
