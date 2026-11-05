// SPDX-License-Identifier: GPL-3.0-only
#include "audio_buffering_panel.h"
#include <QApplication>
#include <QTemporaryDir>
#include <stdexcept>
using namespace soundcurrent::buffering;
void check(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir temp;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    app.setOrganizationName("SoundCurrentBufferingTest");
    app.setApplicationName("Isolated");
    soundcurrent::i18n::Runtime localization;
    localization.initialize(!app.arguments().contains("--language"));
    try {
        check(valid(0) && valid(5) && valid(200) && !valid(4) && !valid(201), "Bounds");
        check(windowsDuration(0) == 400000 && windowsDuration(5) == 50000 && windowsDuration(200) == 2000000,
              "WASAPI duration units");
        check(pollMs(5) == 1 && pollMs(200) == 5, "Polling budget");
        check(requestedMs() == 0 && filterHint().isEmpty(), "Automatic changed Linux defaults");
        int calls = 0;
        auto *widget = panel([&] {
            ++calls;
            if (requestedMs() == 80)
                throw std::runtime_error("Synthetic unavailable buffer");
        });
        widget->resize(550, 220);
        widget->show();
        QApplication::processEvents();
        if (!qEnvironmentVariable("SOUNDCURRENT_BUFFER_SCREENSHOT").isEmpty())
            check(widget->grab().save(qEnvironmentVariable("SOUNDCURRENT_BUFFER_SCREENSHOT")),
                  "Buffer panel screenshot");
        auto *choice = widget->findChild<QComboBox *>("audioBufferMs");
        auto *apply = widget->findChild<QPushButton *>("applyAudioBuffering");
        check(choice && apply && !apply->isEnabled() && calls == 0, "Creating settings restarted audio");
        choice->setCurrentIndex(choice->findData(20));
        check(requestedMs() == 0 && apply->isEnabled(), "Selecting staged value applied early");
        apply->click();
        check(calls == 1 && requestedMs() == 20 && !apply->isEnabled(), "Apply did not commit once");
        check(pipewireLatency(20) == "960/48000" && filterHint().contains("node.latency = 960/48000"),
              "PipeWire request units");
        choice->setCurrentIndex(choice->findData(80));
        apply->click();
        check(calls == 3 && requestedMs() == 20 && choice->currentData().toInt() == 20 && !apply->isEnabled(),
              "Failed restart did not roll back");
        check(widget->findChild<QLabel *>("audioBufferingStatus")
                  ->text()
                  .contains("Synthetic unavailable buffer"),
              "Failure was hidden");
        delete widget;
        QSettings().setValue("audio/bufferMs", 13);
        widget = panel([] {});
        check(widget->findChild<QComboBox *>("audioBufferMs")->currentData().toInt() == 13,
              "Custom saved request lost");
        delete widget;
        QSettings().setValue("audio/bufferMs", 999);
        check(requestedMs() == 0, "Invalid saved request admitted");
    } catch (const std::exception &e) {
        qCritical("%s", e.what());
        return 1;
    }
    return 0;
}
