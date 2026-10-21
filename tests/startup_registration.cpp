// SPDX-License-Identifier: GPL-3.0-only
#include "startup_registration.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDebug>
#include <QUuid>
static void require(bool okay) { if (!okay) qFatal("Startup registration regression"); }
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QTemporaryDir config; require(config.isValid());
#ifdef Q_OS_WIN
    const auto location="HKEY_CURRENT_USER\\Software\\SoundCurrent\\StartupTests\\"+QUuid::createUuid().toString(QUuid::WithoutBraces);
#else
    const auto location=config.path();
#endif
    soundcurrent::StartupRegistration eq(config.filePath("EQ app"),location);
    soundcurrent::StartupRegistration studio(config.filePath("Studio app"),location);
    require(!eq.enabled() && !studio.enabled());
    require(eq.setEnabled(true) && eq.enabled() && !studio.enabled());
    require(studio.setEnabled(true) && studio.enabled() && !eq.enabled());
    require(eq.setEnabled(false) && studio.enabled());
    require(studio.setEnabled(false) && !studio.enabled());
    soundcurrent::StartupRegistration invalid(config.filePath("bad\nname"),location);
    require(!invalid.setEnabled(true));
#ifdef Q_OS_WIN
    QSettings settings(location,QSettings::NativeFormat);settings.remove("");settings.sync();
#endif
    qInfo("PASS: per-user startup enable/disable, exclusive app selection, ownership and unsafe path rejection");
}
