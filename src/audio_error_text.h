// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization_text.h"

namespace soundcurrent::i18n {
// Desktop boundary only. Backend diagnostic strings and processing identifiers
// remain invariant; unknown/external messages retain their original detail.
inline QString audioErrorText(const QString &diagnostic) {
    if (diagnostic == QStringLiteral("The selected EQ settings are invalid"))
        return SC_TR("Invalid equalizer settings");
    if (diagnostic == QStringLiteral("Cable packet exceeds its capture buffer"))
        return SC_TR("Cable packet exceeds its capture buffer");
    if (diagnostic == QStringLiteral("Could not initialize Windows audio COM"))
        return SC_TR("Could not initialize Windows audio COM");
    if (diagnostic == QStringLiteral("Invalid calibration audio"))
        return SC_TR("Invalid calibration audio");
    if (diagnostic == QStringLiteral("Invalid enhancements"))
        return SC_TR("Invalid enhancements");
    if (diagnostic == QStringLiteral("Invalid speaker mix format"))
        return SC_TR("Invalid speaker mix format");
    if (diagnostic == QStringLiteral("Microphone recording consumer stalled"))
        return SC_TR("Microphone recording consumer stalled");
    if (diagnostic == QStringLiteral("SoundCurrent virtual output requires 48 kHz stereo float audio"))
        return SC_TR("SoundCurrent virtual output requires 48 kHz stereo float audio");
    if (diagnostic == QStringLiteral("Unsupported recording format"))
        return SC_TR("Unsupported recording format");
    if (diagnostic == QStringLiteral("Unsupported speaker channel layout or sample rate"))
        return SC_TR("Unsupported speaker channel layout or sample rate");
    if (diagnostic == QStringLiteral("Unsupported speaker mix sample format"))
        return SC_TR("Unsupported speaker mix sample format");
    if (diagnostic == QStringLiteral("Windows audio COM unavailable"))
        return SC_TR("Windows audio COM unavailable");
    return diagnostic;
}
}
