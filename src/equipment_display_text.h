// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization_text.h"
#include <QLocale>
#include <QStringList>
namespace soundcurrent::i18n {
// Translate defined taxonomy keys only; imported custom text is displayed verbatim.
inline QString equipmentTypeText(const QString &key) {
    if(key=="Bookshelf")return SC_TR("Bookshelf speaker");
    if(key=="Center")return SC_TR("Center speaker");
    if(key=="Floorstanding")return SC_TR("Floorstanding speaker");
    if(key=="In-wall")return SC_TR("In-wall speaker");
    if(key=="Unclassified")return SC_TR("Unclassified equipment");
    return key;
}
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
inline QString speakerPolicyText() {
    return SC_TR("Spinorama AutoEQ: correction gain is limited to %1 and Q to %2. Boosts below %3 are omitted. Your listening preset is added separately.")
        .arg(equipmentDisplayData(QStringLiteral("±")+QLocale().toString(6)+" dB",true),
             equipmentDisplayData(QLocale().toString(6),true),
             equipmentDisplayData(QLocale().toString(80)+" Hz",true));
}
inline QString speakerDetailsHeader(const QString &name, const QString &attribution) {
    return equipmentDisplayData(name)+"\n"+
        SC_TR("Measurement: %1").arg(equipmentDisplayData(attribution))+"\n\n"+
        speakerPolicyText()+"\n\n";
}
inline QString speakerFilterLine(double frequency,double gain,double q,const QString &type) {
    return equipmentDisplayData(QLocale().toString(frequency,'g',6)+" Hz",true)+" · "+
        equipmentDisplayData(QLocale().toString(gain,'g',6)+" dB",true)+" · "+
        equipmentDisplayData(QStringLiteral("Q ")+QLocale().toString(q,'g',6),true)+" · "+type+"\n";
}
inline QString measurementSourcesText(const QStringList &sources) {
    QString result;
    for(const auto &source:sources)
        result+=SC_TR("Source: %1").arg(equipmentDisplayData(source,true))+"\n";
    return result;
}

}
