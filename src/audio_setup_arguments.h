// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QStringList>
namespace soundcurrent::i18n {
// QProcess argv values: paths and language tags never become shell code.
inline QStringList audioSetupArguments(const QString &script, bool install, qint64 requester,
                                      const QString &loadedLanguage) {
    return {"-NoProfile", "-ExecutionPolicy", "RemoteSigned", "-File", script,
            install ? "-Install" : "-Settings", "-Quiet", "-RequestingProcessId",
            QString::number(requester), "-Language", loadedLanguage};
}
}
