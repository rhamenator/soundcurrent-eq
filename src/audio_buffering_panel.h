// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "audio_buffering.h"
#include "audio_error_text.h"
#include "localization.h"
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <exception>
#include <functional>
namespace soundcurrent::buffering {
inline int requestedMs() {
    const int value = QSettings().value("audio/bufferMs", 0).toInt();
    return valid(value) ? value : 0;
}
inline QString pipewireLatency(int ms) {
    return ms == 0 || !valid(ms) ? QString() : QString::number(ms * 48) + "/48000";
}
inline QString filterHint() {
    const auto latency = pipewireLatency(requestedMs());
    return latency.isEmpty() ? QString() : " node.latency = " + latency + " ";
}
inline QWidget *panel(const std::function<void()> &restart) {
    auto *box = new QGroupBox(SC_TR("Audio buffering"));
    box->setObjectName("audioBufferingPanel");
    auto *layout = new QVBoxLayout(box);
    auto *choice = new QComboBox;
    choice->setObjectName("audioBufferMs");
    choice->setAccessibleName(SC_TR("Audio buffering"));
    choice->addItem(SC_TR("Automatic"), 0);
    for (int ms : {5, 10, 20, 40, 80, 120, 200})
        choice->addItem(QLocale().toString(ms) + " ms", ms);
    const int previous = requestedMs();
    if (choice->findData(previous) < 0)
        choice->addItem(QLocale().toString(previous) + " ms", previous);
    choice->setCurrentIndex(choice->findData(previous));
    layout->addWidget(choice);
    auto *help = new QLabel(SC_TR(
        "Smaller buffers reduce delay but may cause crackling. Automatic preserves Linux defaults and "
        "requests 40 ms on Windows. Devices may adjust the request. Apply briefly restarts active audio."));
    help->setWordWrap(true);
    choice->setToolTip(help->text());
    layout->addWidget(help);
    auto *apply = new QPushButton(SC_TR("Apply"));
    apply->setObjectName("applyAudioBuffering");
    layout->addWidget(apply);
    auto *status = new QLabel;
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    status->setObjectName("audioBufferingStatus");
    layout->addWidget(status);
    auto refresh = [choice, apply] { apply->setEnabled(choice->currentData().toInt() != requestedMs()); };
    QObject::connect(choice, &QComboBox::currentIndexChanged, box, [refresh] { refresh(); });
    refresh();
    QObject::connect(apply, &QPushButton::clicked, box, [choice, status, restart, refresh] {
        QSettings settings;
        const auto old = settings.value("audio/bufferMs");
        settings.setValue("audio/bufferMs", choice->currentData());
        try {
            restart();
            status->setText(SC_TR("Applied"));
        } catch (const std::exception &error) {
            if (old.isValid())
                settings.setValue("audio/bufferMs", old);
            else
                settings.remove("audio/bufferMs");
            QString message = soundcurrent::i18n::audioErrorText(QString::fromUtf8(error.what()));
            try {
                restart();
            } catch (const std::exception &restore) {
                message += "\n" + soundcurrent::i18n::audioErrorText(QString::fromUtf8(restore.what()));
            }
            status->setText(SC_TR("Audio error: %1").arg(message));
            const QSignalBlocker block(choice);
            choice->setCurrentIndex(choice->findData(requestedMs()));
        }
        refresh();
    });
    return box;
}
} // namespace soundcurrent::buffering
