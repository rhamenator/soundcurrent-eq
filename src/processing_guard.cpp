// SPDX-License-Identifier: GPL-3.0-only
#include "processing_guard.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#endif
namespace soundcurrent {
bool ProcessingGuard::acquire(const QString &directory) {
    if (directory.isEmpty() || !QDir().mkpath(directory)) {
        error_ = "Cannot create the shared SoundCurrent session guard.";
        return false;
    }
    lock_ = std::make_unique<QLockFile>(QDir(directory).filePath("equalizer-session.lock"));
    lock_->setStaleLockTime(0);
    if (lock_->tryLock(0))
        return true;
    qint64 pid = 0;
    QString host, app;
    lock_->getLockInfo(&pid, &host, &app);
    error_ =
        lock_->error() == QLockFile::LockFailedError
            ? "Another SoundCurrent equalizer is running. Quit EQ or Studio before opening the other app."
            : "Cannot acquire the shared SoundCurrent session guard.";
    lock_.reset();
    return false;
}
QString processingGuardDirectory() {
#ifdef Q_OS_WIN
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath("SoundCurrent/shared-runtime");
#else
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return runtime.isEmpty() ? QString() : QDir(runtime).filePath("soundcurrent-shared");
#endif
}
bool recognizedEqualizerProcess(QString name) {
    name = QFileInfo(name).fileName().toLower();
    if (name.endsWith(" (deleted)"))
        name.chop(10);
    return QStringList{
        "soundcurrent-eq", "soundcurrent-studio", "soundcurrent-eq.exe", "soundcurrent-studio.exe",
        "easyeffects",     "pulseeffects",        "fxsound.exe",         "peace.exe"}
        .contains(name);
}
QString equalizerNodeConflict(const QByteArray &dump, const QString &ownPrefix) {
    const auto document = QJsonDocument::fromJson(dump);
    if (!document.isArray())
        return {};
    for (const auto &value : document.array()) {
        const auto node = value.toObject();
        if (node.value("type").toString() != "PipeWire:Interface:Node")
            continue;
        const auto props = node.value("info").toObject().value("props").toObject();
        const auto name = props.value("node.name").toString().toLower();
        if (name == ownPrefix || name.startsWith(ownPrefix + "_") ||
            (ownPrefix == "soundcurrent_eq" &&
             (name == "soundcurrent_mic" || name == "soundcurrent_mic_input")))
            continue;
        const auto app = props.value("application.name").toString().toLower();
        if (name.startsWith("soundcurrent_") || name.contains("easyeffects") ||
            name.contains("pulseeffects") || name.contains("equalizer") || app.contains("easyeffects") ||
            app.contains("pulseeffects"))
            return "Another equalizer route is present: " + props.value("node.description").toString(name) +
                   ". Quit it before using SoundCurrent.";
    }
    return {};
}
QString otherEqualizerConflict(const QString &ownPrefix) {
    const auto own = QCoreApplication::applicationPid();
#ifdef Q_OS_WIN
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return "Cannot inspect running equalizers; SoundCurrent will not enable processing.";
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
        do {
            const auto name = QString::fromWCharArray(entry.szExeFile);
            if (entry.th32ProcessID != own && recognizedEqualizerProcess(name)) {
                CloseHandle(snapshot);
                return name + " is running. Quit it before using SoundCurrent.";
            }
        } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    Q_UNUSED(ownPrefix);
#else
    const QDir proc("/proc");
    for (const auto &id : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool valid = false;
        const auto pid = id.toLongLong(&valid);
        if (!valid || pid == own)
            continue;
        const auto path = QFileInfo("/proc/" + id + "/exe").symLinkTarget();
        if (recognizedEqualizerProcess(path))
            return QFileInfo(path).fileName() + " is running. Quit it before using SoundCurrent.";
    }
    QProcess graph;
    graph.start("pw-dump", {});
    if (graph.waitForStarted(500) && graph.waitForFinished(1500) && graph.exitCode() == 0) {
        const auto bytes = graph.readAllStandardOutput();
        if (bytes.size() <= 16 * 1024 * 1024)
            return equalizerNodeConflict(bytes, ownPrefix);
    }
    if (graph.state() != QProcess::NotRunning) {
        graph.kill();
        graph.waitForFinished(500);
    }
#endif
    return {};
}
} // namespace soundcurrent
