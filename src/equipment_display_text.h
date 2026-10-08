// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization_text.h"
namespace soundcurrent::i18n {
// Directionality belongs to the rendered text, never to imported/saved data.
inline QString equipmentDisplayData(const QString &value, bool leftToRight = false) {
    return QString(QChar(leftToRight ? 0x2066 : 0x2068)) + value + QChar(0x2069);
}
inline QString amplifierPreviewText(const QString &name, const QString &conditions, const QString &source) {
    return equipmentDisplayData(name) + "\n\n" +
        SC_TR("Measurement conditions: %1").arg(equipmentDisplayData(conditions)) + "\n" +
        SC_TR("Source: %1").arg(equipmentDisplayData(source, true)) + "\n\n" +
        SC_TR("Apply only if these conditions match your system.");
}
inline QString amplifierDetailsText(const QString &name, const QString &conditions,
                                    const QString &source, const QString &filters) {
    return equipmentDisplayData(name) + "\n" +
        SC_TR("Measurement conditions: %1").arg(equipmentDisplayData(conditions)) + "\n" +
        SC_TR("Source: %1").arg(equipmentDisplayData(source, true)) + "\n\n" +
        SC_TR("Correction filters:") + "\n" + equipmentDisplayData(filters, true);
}
}
