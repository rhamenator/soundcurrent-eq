// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization_text.h"
#include <QRegularExpression>

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
    // Match only this backend's invariant HRESULT message format. Unknown
    // operations are not guessed, and the hexadecimal error code is preserved.
    static const QRegularExpression failure(QStringLiteral("^(.+) failed \\(0x([0-9A-Fa-f]{1,8})\\)$"));
    const auto match = failure.match(diagnostic);
    if (match.hasMatch()) {
        const auto action = match.captured(1);
        QString translatedAction;
        if (action == QStringLiteral("Automatic audio routing unavailable")) translatedAction = SC_TR("Automatic audio routing unavailable");
        if (action == QStringLiteral("Change default audio endpoint")) translatedAction = SC_TR("Change default audio endpoint");
        if (action == QStringLiteral("Count audio endpoints")) translatedAction = SC_TR("Count audio endpoints");
        if (action == QStringLiteral("Drain test playback")) translatedAction = SC_TR("Drain test playback");
        if (action == QStringLiteral("Enumerate audio devices")) translatedAction = SC_TR("Enumerate audio devices");
        if (action == QStringLiteral("Enumerate endpoints")) translatedAction = SC_TR("Enumerate endpoints");
        if (action == QStringLiteral("Initialize audio capture")) translatedAction = SC_TR("Initialize audio capture");
        if (action == QStringLiteral("Initialize microphone recording")) translatedAction = SC_TR("Initialize microphone recording");
        if (action == QStringLiteral("Initialize speaker output")) translatedAction = SC_TR("Initialize speaker output");
        if (action == QStringLiteral("Initialize test playback")) translatedAction = SC_TR("Initialize test playback");
        if (action == QStringLiteral("List audio endpoints")) translatedAction = SC_TR("List audio endpoints");
        if (action == QStringLiteral("Open audio stream")) translatedAction = SC_TR("Open audio stream");
        if (action == QStringLiteral("Open cable capture stream")) translatedAction = SC_TR("Open cable capture stream");
        if (action == QStringLiteral("Open cable recording endpoint")) translatedAction = SC_TR("Open cable recording endpoint");
        if (action == QStringLiteral("Open endpoint")) translatedAction = SC_TR("Open endpoint");
        if (action == QStringLiteral("Open endpoint volume")) translatedAction = SC_TR("Open endpoint volume");
        if (action == QStringLiteral("Open microphone reader")) translatedAction = SC_TR("Open microphone reader");
        if (action == QStringLiteral("Open speaker endpoint")) translatedAction = SC_TR("Open speaker endpoint");
        if (action == QStringLiteral("Open speaker render stream")) translatedAction = SC_TR("Open speaker render stream");
        if (action == QStringLiteral("Open test playback writer")) translatedAction = SC_TR("Open test playback writer");
        if (action == QStringLiteral("Read audio endpoint")) translatedAction = SC_TR("Read audio endpoint");
        if (action == QStringLiteral("Read audio endpoint ID")) translatedAction = SC_TR("Read audio endpoint ID");
        if (action == QStringLiteral("Read audio endpoint name")) translatedAction = SC_TR("Read audio endpoint name");
        if (action == QStringLiteral("Read audio endpoint properties")) translatedAction = SC_TR("Read audio endpoint properties");
        if (action == QStringLiteral("Read cable audio")) translatedAction = SC_TR("Read cable audio");
        if (action == QStringLiteral("Read cable capture interface")) translatedAction = SC_TR("Read cable capture interface");
        if (action == QStringLiteral("Read cable packet size")) translatedAction = SC_TR("Read cable packet size");
        if (action == QStringLiteral("Read default output ID")) translatedAction = SC_TR("Read default output ID");
        if (action == QStringLiteral("Read default output endpoint")) translatedAction = SC_TR("Read default output endpoint");
        if (action == QStringLiteral("Read microphone mix format")) translatedAction = SC_TR("Read microphone mix format");
        if (action == QStringLiteral("Read microphone packet size")) translatedAction = SC_TR("Read microphone packet size");
        if (action == QStringLiteral("Read microphone samples")) translatedAction = SC_TR("Read microphone samples");
        if (action == QStringLiteral("Read next cable packet size")) translatedAction = SC_TR("Read next cable packet size");
        if (action == QStringLiteral("Read next microphone packet")) translatedAction = SC_TR("Read next microphone packet");
        if (action == QStringLiteral("Read output buffer level")) translatedAction = SC_TR("Read output buffer level");
        if (action == QStringLiteral("Read output level")) translatedAction = SC_TR("Read output level");
        if (action == QStringLiteral("Read output mute")) translatedAction = SC_TR("Read output mute");
        if (action == QStringLiteral("Read speaker level")) translatedAction = SC_TR("Read speaker level");
        if (action == QStringLiteral("Read speaker mix format")) translatedAction = SC_TR("Read speaker mix format");
        if (action == QStringLiteral("Read speaker mute")) translatedAction = SC_TR("Read speaker mute");
        if (action == QStringLiteral("Read speaker render interface")) translatedAction = SC_TR("Read speaker render interface");
        if (action == QStringLiteral("Read speaker volume")) translatedAction = SC_TR("Read speaker volume");
        if (action == QStringLiteral("Read test playback padding")) translatedAction = SC_TR("Read test playback padding");
        if (action == QStringLiteral("Read virtual output mix format")) translatedAction = SC_TR("Read virtual output mix format");
        if (action == QStringLiteral("Release cable audio")) translatedAction = SC_TR("Release cable audio");
        if (action == QStringLiteral("Release microphone packet")) translatedAction = SC_TR("Release microphone packet");
        if (action == QStringLiteral("Release speaker buffer")) translatedAction = SC_TR("Release speaker buffer");
        if (action == QStringLiteral("Release test playback")) translatedAction = SC_TR("Release test playback");
        if (action == QStringLiteral("Set full speaker level for EQ")) translatedAction = SC_TR("Set full speaker level for EQ");
        if (action == QStringLiteral("Set output level")) translatedAction = SC_TR("Set output level");
        if (action == QStringLiteral("Set output mute")) translatedAction = SC_TR("Set output mute");
        if (action == QStringLiteral("Size capture buffer")) translatedAction = SC_TR("Size capture buffer");
        if (action == QStringLiteral("Size output buffer")) translatedAction = SC_TR("Size output buffer");
        if (action == QStringLiteral("Size test playback buffer")) translatedAction = SC_TR("Size test playback buffer");
        if (action == QStringLiteral("Start cable capture")) translatedAction = SC_TR("Start cable capture");
        if (action == QStringLiteral("Start microphone recording")) translatedAction = SC_TR("Start microphone recording");
        if (action == QStringLiteral("Start speaker output")) translatedAction = SC_TR("Start speaker output");
        if (action == QStringLiteral("Start test playback")) translatedAction = SC_TR("Start test playback");
        if (action == QStringLiteral("Unmute speaker for EQ")) translatedAction = SC_TR("Unmute speaker for EQ");
        if (action == QStringLiteral("Write speaker buffer")) translatedAction = SC_TR("Write speaker buffer");
        if (action == QStringLiteral("Write test playback")) translatedAction = SC_TR("Write test playback");
        if (!translatedAction.isEmpty())
            return SC_TR("%1 failed (0x%2)").arg(translatedAction, match.captured(2));
    }
    return diagnostic;
}
}
