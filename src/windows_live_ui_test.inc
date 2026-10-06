// Actual Windows default-route change while the live UI is processing.
if (app.arguments().contains("--windows-live-manual-route-test")) {
    try {
        soundcurrent::ProcessingGuard guard;
        if (!guard.acquire(soundcurrent::processingGuardDirectory())) throw std::runtime_error("Audio session busy");
        const auto owned=standardCable(false).toStdWString();
        const auto outputs=devices();
        if (outputs.isEmpty()) throw std::runtime_error("No physical fixture");
        auto physical=outputs.front().name.toStdWString();
        const auto current=soundcurrent::windowsDefaultOutputId();
        for(const auto &device:outputs) if(device.name.toStdWString()==current) physical=current;
        const bool otherOutput=app.arguments().contains("--other-physical-output");
        QString selected;
        if(otherOutput) {
            for(const auto &device:outputs) if(device.name.toStdWString()!=physical) {selected=device.name;break;}
            if(selected.isEmpty()) throw std::runtime_error("Need two physical outputs");
        }
        int result=0;
        {
            MainWindow window(true);
            for(auto *button:window.findChildren<QPushButton *>())
                if(button->text()=="Refresh devices") {button->click();break;}
            auto *power=window.findChild<QCheckBox *>("powerToggle");
            if (!power) throw std::runtime_error("Power control missing");
            if(otherOutput) {
                QComboBox *output=nullptr;
                for(auto *box:window.findChildren<QComboBox *>())
                    if(box->accessibleName()=="Output device") {output=box;break;}
                if(!output || output->findData(selected)<0) throw std::runtime_error("Output control missing");
                output->setCurrentIndex(output->findData(selected));
            }
            QTimer::singleShot(500,&window,[&] {
                try {
                    if (!power->isChecked()) throw std::runtime_error("Live audio did not start");
                    for(int role=0;role<3;++role)
                        if(soundcurrent::windowsDefaultEndpointId(false,role)!=owned) throw std::runtime_error("Route not acquired");
                    // Change actual endpoint defaults, as an external Windows
                    // device choice would; avoid changing volume in this call.
                    if(!soundcurrent::windowsRestoreOwnedRoute(false,owned,physical,{physical,physical,physical},false))
                        throw std::runtime_error("Cannot select direct physical output");
                } catch(const std::exception &e){qCritical("Manual selection: %s",e.what());result=1;app.quit();}
            });
            QTimer::singleShot(4000,&window,[&] {
                try {
                    if(power->isChecked()) throw std::runtime_error("UI still claims EQ on after Windows selects direct physical output");
                    for(int role=0;role<3;++role)
                        if(soundcurrent::windowsDefaultEndpointId(false,role)!=physical) throw std::runtime_error("Manual output choice overwritten");
                } catch(const std::exception &e){qCritical("Manual selection: %s",e.what());result=1;}
                app.quit();
            });
            app.exec();
        }
        if(result==0) QTextStream(stdout)<<"PASS: direct Windows output choice disables processing and preserves all roles"<<Qt::endl;
        return result;
    } catch(const std::exception &e){qCritical("Manual route test: %s",e.what());return 1;}
}
// Opt-in test coordinated by a host-side unplug/replug fixture.
if (app.arguments().contains("--windows-live-hotplug-test")) {
    try {
        if (app.arguments().size()!=3) throw std::runtime_error("Supply a test status file");
        soundcurrent::ProcessingGuard guard;
        if (!guard.acquire(soundcurrent::processingGuardDirectory())) throw std::runtime_error("Audio session busy");
        const auto owned=standardCable(false).toStdWString();
        auto report=[&](const QString &value) {
            QSaveFile file(app.arguments()[2]);
            if (!file.open(QIODevice::WriteOnly) || file.write(value.toUtf8())<0 || !file.commit())
                throw std::runtime_error("Cannot report hotplug test state");
        };
        int result=0;
        {
            // Exercise the production live monitor, not offline preview mode.
            MainWindow window(true);
            for (auto *button:window.findChildren<QPushButton *>())
                if (button->text()=="Refresh devices") {button->click();break;}
            QComboBox *output=nullptr;
            for (auto *box:window.findChildren<QComboBox *>())
                if (box->accessibleName()=="Output device") {output=box;break;}
            auto *power=window.findChild<QCheckBox *>("powerToggle");
            auto *status=window.findChild<QLabel *>("status");
            if (!output || !power || !status) throw std::runtime_error("Live controls unavailable");
            const QString usb="Speakers (Audio Device)", onboard="Speakers (High Definition Audio Device)";
            const int usbIndex=output->findText(usb);
            if (usbIndex<0 || output->findText(onboard)<0) throw std::runtime_error("Need clone USB and onboard fixtures");
            output->setCurrentIndex(usbIndex);
            power->setChecked(true);
            if (!power->isChecked() || !status->text().contains(usb)) throw std::runtime_error("USB processing did not start");
            report("ready");
            int stage=0;QElapsedTimer elapsed;elapsed.start();QTimer poll;poll.setInterval(100);
            QObject::connect(&poll,&QTimer::timeout,&window,[&] {
                try {
                    if (elapsed.elapsed()>60000) {
                        QStringList names;
                        for (int i=0;i<output->count();++i) names.append(output->itemText(i));
                        throw std::runtime_error((QString("Hotplug test timed out; stage=%1; status=%2; outputs=%3")
                            .arg(stage).arg(status->text(),names.join(" | "))).toStdString());
                    }
                    if (!power->isChecked()) throw std::runtime_error("Processing disabled during hotplug");
                    if (stage==0 && output->findText(usb)<0 && status->text().contains(onboard)) {
                        if (!output->currentData().toString().isEmpty()) throw std::runtime_error("Unplug did not switch to automatic");
                        for (int role=0;role<3;++role)
                            if (soundcurrent::windowsDefaultEndpointId(false,role)!=owned)
                                throw std::runtime_error("Owned output role missing after completed switch");
                        report("fallback");stage=1;
                    } else if (stage==1 && output->findText(usb)>=0 && status->text().contains(usb)) {
                        for (int role=0;role<3;++role)
                            if (soundcurrent::windowsDefaultEndpointId(false,role)!=owned)
                                throw std::runtime_error("Owned output role missing after completed switch");
                        report("reconnected");poll.stop();power->setChecked(false);app.quit();
                    }
                } catch (const std::exception &error) {
                    result=1;poll.stop();report(QString("FAIL: ")+error.what());app.quit();
                }
            });
            poll.start();app.exec();
        }
        for (int role=0;role<3;++role)
            if (soundcurrent::windowsDefaultEndpointId(false,role)==owned) throw std::runtime_error("Hotplug test left owned default");
        return result;
    } catch (const std::exception &error) { qCritical("Hotplug test: %s",error.what());return 1; }
}
// SPDX-License-Identifier: GPL-3.0-only
// Included inside main(): opt-in isolated Windows clone only.
if (app.arguments().contains("--windows-live-ui-test")) {
    try {
        soundcurrent::ProcessingGuard guard;
        if (!guard.acquire(soundcurrent::processingGuardDirectory()))
            throw std::runtime_error("Another SoundCurrent session is active");
        const auto physical = devices();
        if (physical.isEmpty()) throw std::runtime_error("No physical output fixture");
        const auto owned = standardCable(false).toStdWString();
        std::array<std::wstring,3> before;
        for (int role=0; role<3; ++role) {
            before[role]=soundcurrent::windowsDefaultEndpointId(false,role);
            if (before[role]==owned) throw std::runtime_error("Fixture already owns default output; preserve it");
        }
        auto verify = [&](bool enabled) {
            for (int role=0; role<3; ++role)
                if (soundcurrent::windowsDefaultEndpointId(false,role)!=(enabled?owned:before[role]))
                    throw std::runtime_error("UI output route does not match enabled state");
        };
        {
            MainWindow window(false);
            for (auto *button:window.findChildren<QPushButton *>())
                if (button->text()=="Refresh devices") {button->click();break;}
            auto *power=window.findChild<QCheckBox *>("powerToggle");
            if (!power || !power->isEnabled()) throw std::runtime_error("Output control unavailable");
            power->setChecked(true);
            if (!power->isChecked()) throw std::runtime_error("UI failed to start audio");
            verify(true);
            power->setChecked(false);
            verify(false);
            power->setChecked(true);
            verify(true);
            QPushButton *quit=nullptr;
            for (auto *button:window.findChildren<QPushButton *>())
                if (button->text()=="Quit app") {quit=button;break;}
            if (!quit) throw std::runtime_error("Quit control unavailable");
            QTimer::singleShot(0,quit,&QPushButton::click);
            if (app.exec()!=0) throw std::runtime_error("Quit event loop failed");
        }
        verify(false);
        qInfo("PASS: live UI enable, disable, re-enable and Quit restored all output roles");
        return 0;
    } catch (const std::exception &error) {
        qCritical("Live UI route test failed: %s",error.what());
        return 1;
    }
}
