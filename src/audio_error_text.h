// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization_text.h"

namespace soundcurrent::i18n {
// Desktop boundary only. Backend diagnostic strings and processing identifiers
// remain invariant; unknown/external messages retain their original detail.
inline QString audioErrorText(const QString &diagnostic) {
    if (diagnostic == QStringLiteral("The selected EQ settings are invalid"))
        return SC_TR("Invalid equalizer settings");
    return diagnostic;
}
}
