// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QCoreApplication>
#include <QLocale>
namespace soundcurrent::i18n {
inline QString text(const char *source, int n = -1) {
    return QCoreApplication::translate("SoundCurrent", source, nullptr, n);
}
// Display formatting only: callers keep numeric processing and state unchanged.
inline QString estimatedBandLevelText(const QString &frequency,double db) {
    return text("Estimated output near %1: %2 dBFS").arg(frequency,QLocale().toString(db,'f',1));
}
inline QString calibrationBandPreviewText(int frequency,double measured,double suggested) {
    return text("%1 Hz: measured %2%3 dB; suggested %4%5 dB")
        .arg(QLocale().toString(frequency),measured>0?QStringLiteral("+"):QString(),
             QLocale().toString(measured,'f',1),suggested>0?QStringLiteral("+"):QString(),
             QLocale().toString(suggested,'f',1));
}
inline QString balancePositionText(int value) {
    if(value==0)return text("Center");
    return QStringLiteral("%1 %2%3").arg(text(value<0?"L":"R"),
        QLocale().toString(value<0?-value:value),QLocale().percent());
}
// Recognize the localized worker diagnostic without an English-prefix test.
// Match fixed parts on both sides so placeholder order can vary by language.
inline bool isCalibrationFailureMessage(const QString &message) {
    const auto pattern=text("Measurement failed: %1");
    const auto position=pattern.indexOf("%1");
    return position>=0 && message.size()>pattern.size()-2 &&
           message.startsWith(pattern.left(position)) && message.endsWith(pattern.mid(position+2));
}
// Keep a signed number and its invariant unit together inside RTL prose.
// Directional isolates are introduced only at display time, never in catalogs
// or persisted parameter values. The translated sentence keeps its own order.
inline QString numberWithUnit(QString pattern, const QString &number, const QString &unit) {
    const QString token = QStringLiteral("%1 ") + unit;
    pattern.replace(token, QString(QChar(0x2066)) + token + QChar(0x2069));
    return pattern.arg(number);
}
}
#define SC_TR(source) ::soundcurrent::i18n::text(source)
