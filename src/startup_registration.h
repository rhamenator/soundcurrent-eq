// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

namespace soundcurrent {
// One shared registration selects either EQ or Studio, never both. All changes
// are per-user. An injected config directory keeps tests out of real login state.
class StartupRegistration {
public:
    explicit StartupRegistration(QString executable, QString testConfig = {})
        : executable_(QFileInfo(executable).absoluteFilePath()), testConfig_(testConfig) {}
    QString command() const { return QStringLiteral("\"") + executable_ + QStringLiteral("\" --background"); }
    bool enabled() const {
#ifdef Q_OS_WIN
        QSettings settings(registryPath(), QSettings::NativeFormat);
        return settings.value("SoundCurrent").toString() == command();
#else
        QFile file(entryPath());
        if (!file.open(QIODevice::ReadOnly)) return false;
        const auto bytes = file.read(16385);
        return bytes.size() <= 16384 && bytes == desktopEntry().toUtf8();
#endif
    }
    bool setEnabled(bool on) const {
        if (executable_.contains('\n') || executable_.contains('\r') || executable_.contains('"')) return false;
#ifdef Q_OS_WIN
        QSettings settings(registryPath(), QSettings::NativeFormat);
        if (on) settings.setValue("SoundCurrent", command());
        else if (settings.value("SoundCurrent").toString() == command()) settings.remove("SoundCurrent");
        settings.sync();
        return settings.status() == QSettings::NoError;
#else
        if (!QDir().mkpath(QFileInfo(entryPath()).absolutePath())) return false;
        QLockFile lock(entryPath() + ".lock");
        if (!lock.tryLock(0)) return false;
        if (!on) return !enabled() || QFile::remove(entryPath());
        QSaveFile file(entryPath());
        if (!file.open(QIODevice::WriteOnly)) return false;
        const auto bytes = desktopEntry().toUtf8();
        if (file.write(bytes) != bytes.size()) return false;
        return file.commit();
#endif
    }
private:
#ifdef Q_OS_WIN
    QString registryPath() const {
        return testConfig_.isEmpty()
            ? QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run")
            : testConfig_; // tests supply their own temporary registry key
    }
#else
    QString entryPath() const {
        const auto config = testConfig_.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) : testConfig_;
        return QDir(config).filePath("autostart/soundcurrent.desktop");
    }
    QString desktopEntry() const {
        QString escaped = executable_;
        escaped.replace("\\", "\\\\");
        escaped.replace("`", "\\`");
        escaped.replace("$", "\\$");
        escaped.replace("%", "%%");
        // Desktop entry string escapes are decoded before Exec quoting.
        QString exec = QStringLiteral("\"") + escaped + "\" --background";
        exec.replace("\\", "\\\\");
        const auto tryExec = QString(executable_).replace("\\", "\\\\");
        return "[Desktop Entry]\nType=Application\nName=SoundCurrent\nExec=" + exec
            + "\nTryExec=" + tryExec + "\nTerminal=false\n";
    }
#endif
    QString executable_, testConfig_;
};
}
