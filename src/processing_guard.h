// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QLockFile>
#include <QString>
#include <memory>
namespace soundcurrent {
class ProcessingGuard {
  public:
    bool acquire(const QString &directory);
    QString error() const { return error_; }

  private:
    std::unique_ptr<QLockFile> lock_;
    QString error_;
};
QString processingGuardDirectory();
bool recognizedEqualizerProcess(QString name);
QString equalizerNodeConflict(const QByteArray &dump, const QString &ownPrefix);
QString otherEqualizerConflict(const QString &ownPrefix);
} // namespace soundcurrent
