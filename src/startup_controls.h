// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "startup_registration.h"
#include "localization_text.h"
#include <QCoreApplication>
#include <QGroupBox>
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVBoxLayout>
namespace soundcurrent {
inline QWidget *startupPanel() {
    auto *panel = new QGroupBox(SC_TR("Startup"));
    auto *layout = new QVBoxLayout(panel);
    auto *toggle = new QCheckBox(SC_TR("Start when I sign in"));
    toggle->setObjectName("startAtLogin");
    const StartupRegistration registration(QCoreApplication::applicationFilePath());
    toggle->setChecked(registration.enabled());
    auto *help = new QLabel(SC_TR("Only one SoundCurrent app starts at sign-in. Enabling this replaces the other app's startup setting. It starts in the background when a tray icon is available."));
    help->setWordWrap(true);
    layout->addWidget(toggle);layout->addWidget(help);
    QObject::connect(toggle,&QCheckBox::toggled,panel,[toggle,panel,registration](bool enabled){
        if (!registration.setEnabled(enabled)) {
            const QSignalBlocker blocked(toggle);toggle->setChecked(registration.enabled());
            QMessageBox::warning(panel,SC_TR("Startup"),SC_TR("Could not update startup settings."));
        }
    });
    return panel;
}
}
