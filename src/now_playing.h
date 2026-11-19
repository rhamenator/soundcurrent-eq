// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QString>
namespace soundcurrent {
// Text for what the desktop media player is playing right now ("Artist — Title"), or an empty string.
// Linux reads the standard MPRIS interface on the session bus (read-only); other platforms return an empty string.
QString nowPlayingText();
}
