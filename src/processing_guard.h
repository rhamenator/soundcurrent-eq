// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QLockFile>
#include <QString>
#include <memory>
#ifdef Q_OS_WIN
#include "windows_audio_session_gate.h"
#endif
namespace soundcurrent {
class ProcessingGuard {
  public:
    bool acquire(const QString &directory);
    QString error() const { return error_; }

  private:
#ifdef Q_OS_WIN
    WindowsAudioSessionGate sessionGate_;
#endif
    std::unique_ptr<QLockFile> lock_;
    QString error_;
};
QString processingGuardDirectory();
bool recognizedEqualizerProcess(QString name);
QString equalizerNodeConflict(const QByteArray &dump, const QString &ownPrefix);
QString otherEqualizerConflict(const QString &ownPrefix);
} // namespace soundcurrent
