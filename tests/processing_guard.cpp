// SPDX-License-Identifier: GPL-3.0-only
#include "processing_guard.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <iostream>
#include <stdexcept>
using namespace soundcurrent;
void check(bool ok, const char *reason) {
    if (!ok)
        throw std::runtime_error(reason);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (app.arguments().size() == 3 && app.arguments()[1] == "--hold") {
        ProcessingGuard guard;
        if (!guard.acquire(app.arguments()[2]))
            return 2;
        QTextStream(stdout) << "ready" << Qt::endl;
        QTimer::singleShot(60000, &app, &QCoreApplication::quit);
        return app.exec();
    }
    try {
#ifdef Q_OS_WIN
        if (app.arguments().size() == 3 && app.arguments()[1] == "--expect-process-conflict") {
            const auto reason = otherEqualizerConflict("soundcurrent_test");
            check(reason.contains(app.arguments()[2], Qt::CaseInsensitive),
                  "live Windows process conflict diagnostic missing");
            std::cout << "PASS: live Windows process conflict detected\n";
            return 0;
        }
        if (app.arguments().size() == 2 && app.arguments()[1] == "--expect-no-process-conflict") {
            check(otherEqualizerConflict("soundcurrent_test").isEmpty(),
                  "unexpected Windows equalizer process conflict");
            std::cout << "PASS: no live Windows process conflict\n";
            return 0;
        }
#endif
        check(recognizedEqualizerProcess("FxSound.EXE") &&
                  recognizedEqualizerProcess("soundcurrent-eq (deleted)") &&
                  recognizedEqualizerProcess("easyeffects"),
              "known equalizer identities");
        check(!recognizedEqualizerProcess("audiodg.exe") && !recognizedEqualizerProcess("pipewire"),
              "ordinary audio infrastructure not equalizer");
        auto graph = [](const QString &name) {
            return QJsonDocument(QJsonArray{QJsonObject{
                                     {"type", "PipeWire:Interface:Node"},
                                     {"info", QJsonObject{{"props", QJsonObject{{"node.name", name}}}}}}})
                .toJson();
        };
        check(!equalizerNodeConflict(graph("soundcurrent_studio"), "soundcurrent_eq").isEmpty(),
              "other SoundCurrent route");
        check(!equalizerNodeConflict(graph("easyeffects_sink"), "soundcurrent_eq").isEmpty(),
              "third party route");
        check(equalizerNodeConflict(graph("soundcurrent_eq_output"), "soundcurrent_eq").isEmpty() &&
                  equalizerNodeConflict(graph("alsa_output.usb"), "soundcurrent_eq").isEmpty(),
              "own / hardware routes allowed");
        QTemporaryDir directory;
        check(directory.isValid(), "test runtime");
        QProcess owner;
        owner.start(QCoreApplication::applicationFilePath(), {"--hold", directory.path()});
        check(owner.waitForStarted(30000), "owner starts");
        check(owner.waitForReadyRead(30000) && owner.readAllStandardOutput().contains("ready"),
              "owner acquired shared lock");
        {
            ProcessingGuard contender;
            check(!contender.acquire(directory.path()) && !contender.error().isEmpty(), "second app refused");
        }
#ifdef Q_OS_WIN
        {
            QTemporaryDir otherUserDirectory;
            ProcessingGuard otherUser;
            check(otherUserDirectory.isValid() && !otherUser.acquire(otherUserDirectory.path()),
                  "different per-user directory still refused by global session gate");
        }
#endif
        owner.kill();
        check(owner.waitForFinished(30000), "owner crash completes");
        {
            ProcessingGuard recovery;
            check(recovery.acquire(directory.path()), "dead process lock recovers");
        }
        {
            ProcessingGuard released;
            check(released.acquire(directory.path()), "normal exit releases lock");
        }
        std::cout << "Processing guard: cross-process exclusion, crash recovery, release and recognized "
                     "process/route coverage passed.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
