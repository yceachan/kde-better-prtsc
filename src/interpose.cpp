#include <QtCore/QVariantMap>
#include <QtCore/QtGlobal>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCall>
#include <QtDBus/QDBusUnixFileDescriptor>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <optional>

namespace {

[[noreturn]] void fail(const char *message)
{
    std::fprintf(stderr, "snipaste-kwin: %s\n", message);
    std::abort();
}

template<typename Function>
Function resolve(const char *symbol)
{
    dlerror();
    void *address = dlsym(RTLD_NEXT, symbol);
    const char *error = dlerror();
    if (error || !address)
        fail(error ? error : "original Qt D-Bus function not found");
    return reinterpret_cast<Function>(address);
}

void checkRuntime()
{
    static const bool checked = [] {
        if (std::strcmp(qVersion(), "6.2.4") != 0)
            fail("Qt 6.2.4 runtime required; use scripts/run-snipaste.sh");
        return true;
    }();
    (void)checked;
}

std::optional<QDBusMessage> patchRequest(const QDBusMessage &message)
{
    if (message.type() != QDBusMessage::MethodCallMessage
        || message.service() != QLatin1String("org.kde.KWin.ScreenShot2")
        || message.path() != QLatin1String("/org/kde/KWin/ScreenShot2")
        || message.interface() != QLatin1String("org.kde.KWin.ScreenShot2")
        || message.member() != QLatin1String("CaptureWorkspace")) {
        return std::nullopt;
    }

    auto arguments = message.arguments();
    if (arguments.size() != 2
        || arguments[0].userType() != QMetaType::QVariantMap
        || arguments[1].userType() != qMetaTypeId<QDBusUnixFileDescriptor>()) {
        fail("unexpected CaptureWorkspace signature; patch needs review");
    }

    auto options = arguments[0].toMap();
    static const bool trace = [] {
        const char *value = std::getenv("SNIPASTE_PATCH_TRACE");
        return value && std::strcmp(value, "1") == 0;
    }();
    if (trace) {
        const auto it = options.constFind(QStringLiteral("hide-caller-windows"));
        const char *previous = it == options.cend() ? "absent" : (it->toBool() ? "true" : "false");
        std::fprintf(stderr, "snipaste-kwin: CaptureWorkspace hide-caller-windows=%s -> false\n",
                     previous);
    }
    options.insert(QStringLiteral("hide-caller-windows"), false);
    arguments[0] = options;

    // QDBusMessage copies share mutable data: create a fresh call, not a shallow copy.
    auto patched = QDBusMessage::createMethodCall(message.service(), message.path(),
                                                 message.interface(), message.member());
    patched.setAutoStartService(message.autoStartService());
    patched.setInteractiveAuthorizationAllowed(message.isInteractiveAuthorizationAllowed());
    patched.setArguments(arguments);
    return patched;
}

} // namespace

QDBusMessage QDBusConnection::call(const QDBusMessage &message, QDBus::CallMode mode,
                                  int timeout) const
{
    checkRuntime();
    using Original = QDBusMessage (*)(const QDBusConnection *, const QDBusMessage &,
                                      QDBus::CallMode, int);
    static const auto original = resolve<Original>(
        "_ZNK15QDBusConnection4callERK12QDBusMessageN5QDBus8CallModeEi");
    const auto patched = patchRequest(message);
    return original(this, patched ? *patched : message, mode, timeout);
}

QDBusPendingCall QDBusConnection::asyncCall(const QDBusMessage &message, int timeout) const
{
    checkRuntime();
    using Original = QDBusPendingCall (*)(const QDBusConnection *, const QDBusMessage &, int);
    static const auto original = resolve<Original>(
        "_ZNK15QDBusConnection9asyncCallERK12QDBusMessagei");
    const auto patched = patchRequest(message);
    return original(this, patched ? *patched : message, timeout);
}
