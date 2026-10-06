// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "windows_audio.h"
#include <QCoreApplication>
#include <QProcess>
#include <QString>
#include <memory>
#include <functional>
#include <utility>
#include <stdexcept>

namespace soundcurrent {
// A ready recovery child must exist before any default endpoint is changed.
class ManagedWindowsRoute {
public:
    ManagedWindowsRoute(bool capture, const std::wstring &owned,
                        const std::wstring &fallback, bool volume = false,
                        std::function<void()> stopBridge = {})
        : stopBridge_(std::move(stopBridge)) {
        // Stop processing before restoring defaults when the helper dies.
        // The callback must not throw or destroy this route object.
        QObject::connect(&guardian_, &QProcess::finished, &guardian_,
            [this](int, QProcess::ExitStatus) {
                if (!route_) return;
                if (stopBridge_) stopBridge_();
                route_.reset();
            });
        route_ = std::make_unique<WindowsRouteLease>(capture, owned, fallback, volume,
            [&](const std::array<std::wstring,3> &originals) {
                guardian_.setProgram(QCoreApplication::applicationDirPath() + "/soundcurrent-route-guardian.exe");
                guardian_.setArguments({capture ? "1" : "0", QString::fromStdWString(owned),
                    QString::fromStdWString(fallback), QString::fromStdWString(originals[0]),
                    QString::fromStdWString(originals[1]), QString::fromStdWString(originals[2]),
                    "1", volume ? "1" : "0"});
                guardian_.start();
                if (!guardian_.waitForStarted(2000) || !guardian_.waitForReadyRead(3000) ||
                    guardian_.readAllStandardOutput().trimmed() != "ready")
                    throw std::runtime_error("Audio route recovery helper could not start. Repair or reinstall SoundCurrent.");
            });
    }
    ~ManagedWindowsRoute() {
        route_.reset();
        guardian_.closeWriteChannel();
        guardian_.waitForFinished(3000);
    }
    bool healthy() const { return route_ && guardian_.state() == QProcess::Running; }
private:
    std::function<void()> stopBridge_;
    QProcess guardian_;
    std::unique_ptr<WindowsRouteLease> route_;
};

} // namespace soundcurrent
