// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QStringList>
#include <QByteArray>
namespace soundcurrent::i18n {
// Helpers emit UTF-8; decode only after all chunks have been accumulated.
inline QString audioSetupOutput(const QByteArray &bytes) { return QString::fromUtf8(bytes).trimmed(); }
// QProcess argv values: paths and language tags never become shell code.
inline QStringList audioSetupArguments(const QString &script, bool install, qint64 requester,
                                      const QString &loadedLanguage) {
    return {"-NoProfile", "-ExecutionPolicy", "RemoteSigned", "-File", script,
            install ? "-Install" : "-Settings", "-Quiet", "-RequestingProcessId",
            QString::number(requester), "-Language", loadedLanguage};
}
}
