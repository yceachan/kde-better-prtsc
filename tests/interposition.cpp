#include <QtCore/QCoreApplication>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusUnixFileDescriptor>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

void require(bool condition, const char *description)
{
    if (!condition)
        throw std::runtime_error(description);
}

QDBusMessage request(const QVariantMap &options, const QDBusUnixFileDescriptor &fd,
                     const QString &service = QStringLiteral("org.kde.KWin.ScreenShot2"),
                     const QString &path = QStringLiteral("/org/kde/KWin/ScreenShot2"),
                     const QString &interface = QStringLiteral("org.kde.KWin.ScreenShot2"),
                     const QString &member = QStringLiteral("CaptureWorkspace"))
{
    auto message = QDBusMessage::createMethodCall(service, path, interface, member);
    message.setAutoStartService(false);
    message.setInteractiveAuthorizationAllowed(true);
    message.setArguments({options, QVariant::fromValue(fd)});
    return message;
}

void verify(const QDBusMessage &input, const QDBusMessage &reply,
            const QVariantMap &expected, int mode)
{
    const auto arguments = reply.arguments();
    require(arguments.size() == 3, "argument count");
    require(arguments[0].toMap() == expected, "options changed incorrectly");
    const auto before = input.arguments()[1].value<QDBusUnixFileDescriptor>();
    const auto after = arguments[1].value<QDBusUnixFileDescriptor>();
    struct stat beforeStat{}, afterStat{};
    require(fstat(before.fileDescriptor(), &beforeStat) == 0, "input fd closed");
    require(fstat(after.fileDescriptor(), &afterStat) == 0, "output fd closed");
    require(beforeStat.st_dev == afterStat.st_dev && beforeStat.st_ino == afterStat.st_ino,
            "file descriptor changed");
    const auto metadata = arguments[2].toMap();
    require(metadata[QStringLiteral("service")].toString() == input.service(), "service");
    require(metadata[QStringLiteral("path")].toString() == input.path(), "path");
    require(metadata[QStringLiteral("interface")].toString() == input.interface(), "interface");
    require(metadata[QStringLiteral("member")].toString() == input.member(), "member");
    require(!metadata[QStringLiteral("autostart")].toBool(), "autostart flag lost");
    require(metadata[QStringLiteral("authorization")].toBool(), "authorization flag lost");
    require(metadata[QStringLiteral("mode")].toInt() == mode, "call mode");
    require(metadata[QStringLiteral("timeout")].toInt() == 1234, "timeout");
}

void verifyBoth(const QDBusConnection &connection, const QDBusMessage &input,
                const QVariantMap &expected)
{
    const auto original = input.arguments();
    verify(input, connection.call(input, QDBus::Block, 1234), expected,
           static_cast<int>(QDBus::Block));
    require(input.arguments() == original, "synchronous call mutated original message");
    const QDBusMessage reply = connection.asyncCall(input, 1234).reply();
    verify(input, reply, expected, -1);
    require(input.arguments() == original, "asynchronous call mutated original message");
}

void benchmark(const QDBusConnection &connection, const QDBusMessage &input)
{
    constexpr int iterations = 10000;
    std::vector<double> samples;
    for (int sample = 0; sample < 9; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i)
            (void)connection.asyncCall(input, 1234);
        const auto elapsed = std::chrono::steady_clock::now() - start;
        samples.push_back(std::chrono::duration<double, std::micro>(elapsed).count() / iterations);
    }
    std::sort(samples.begin(), samples.end());
    std::printf("mock transport async call: median %.3f us/call (9 x %d calls)\n",
                samples[samples.size() / 2], iterations);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        require(std::strcmp(qVersion(), "6.2.4") == 0, "wrong test Qt runtime");
        require(argc >= 2, "expected baseline or patched");
        const bool patched = std::strcmp(argv[1], "patched") == 0;
        require(patched || std::strcmp(argv[1], "baseline") == 0, "unknown test mode");
        int pipeFds[2];
        require(pipe(pipeFds) == 0, "pipe");
        const QDBusUnixFileDescriptor fd(pipeFds[0]);
        close(pipeFds[0]);
        close(pipeFds[1]);
        const QDBusConnection connection(QStringLiteral("mock-transport"));

        const QVariantMap options{{QStringLiteral("include-cursor"), true},
                                  {QStringLiteral("native-resolution"), true}};
        for (int state = 0; state < 3; ++state) {
            auto original = options;
            if (state != 0)
                original.insert(QStringLiteral("hide-caller-windows"), state == 1);
            auto expected = original;
            if (patched)
                expected.insert(QStringLiteral("hide-caller-windows"), false);
            verifyBoth(connection, request(original, fd), expected);
        }

        // Each routing field must match; no unrelated method or destination is rewritten.
        const auto service = QStringLiteral("org.kde.KWin.ScreenShot2");
        const auto path = QStringLiteral("/org/kde/KWin/ScreenShot2");
        verifyBoth(connection, request(options, fd, QStringLiteral("org.example.Other")), options);
        verifyBoth(connection, request(options, fd, service, QStringLiteral("/other")), options);
        verifyBoth(connection, request(options, fd, service, path, QStringLiteral("other")), options);
        verifyBoth(connection, request(options, fd, service, path, service,
                                       QStringLiteral("CaptureWindow")), options);
        const auto message = request(options, fd);
        std::printf("PASS: %s; sync/async, exact match, original message, flags, timeout, fd\n",
                    patched ? "preloaded patch" : "baseline");
        if (argc == 3 && std::strcmp(argv[2], "--benchmark") == 0)
            benchmark(connection, message);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
