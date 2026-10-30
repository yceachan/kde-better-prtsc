#include <QtCore/QVariantMap>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>

namespace {

QDBusMessage echo(const QDBusMessage &message, int mode, int timeout)
{
    const QVariantMap metadata{
        {QStringLiteral("service"), message.service()},
        {QStringLiteral("path"), message.path()},
        {QStringLiteral("interface"), message.interface()},
        {QStringLiteral("member"), message.member()},
        {QStringLiteral("autostart"), message.autoStartService()},
        {QStringLiteral("authorization"), message.isInteractiveAuthorizationAllowed()},
        {QStringLiteral("mode"), mode},
        {QStringLiteral("timeout"), timeout},
    };
    auto arguments = message.arguments();
    arguments.append(metadata);
    return message.createReply(arguments);
}

} // namespace

QDBusMessage QDBusConnection::call(const QDBusMessage &message, QDBus::CallMode mode,
                                  int timeout) const
{
    return echo(message, static_cast<int>(mode), timeout);
}

QDBusPendingCall QDBusConnection::asyncCall(const QDBusMessage &message, int timeout) const
{
    return QDBusPendingCall::fromCompletedCall(echo(message, -1, timeout));
}
