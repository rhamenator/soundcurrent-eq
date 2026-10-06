// SPDX-License-Identifier: GPL-3.0-only
// Opt-in hooks inside the ordinary application startup and conflict monitor.
if(app.arguments().contains("--windows-live-conflict-test")) {
    if(app.arguments().size()!=3) {qCritical("Supply conflict test status path");return 1;}
    const auto phase=app.arguments()[2];
    QTimer::singleShot(1000,&window,[&,phase] {
        try {
            auto *power=window.findChild<QCheckBox *>("powerToggle");
            if(!power || !power->isChecked()) throw std::runtime_error("Live processing did not start");
            const auto owned=standardCable(false).toStdWString();
            for(int role=0;role<3;++role)
                if(soundcurrent::windowsDefaultEndpointId(false,role)!=owned)
                    throw std::runtime_error("Live app did not acquire all output roles");
            QSaveFile file(phase);
            if(!file.open(QIODevice::WriteOnly) || file.write("ready")!=5 || !file.commit())
                throw std::runtime_error("Cannot write conflict readiness");
        }catch(const std::exception &error){qCritical("Conflict readiness: %s",error.what());app.exit(1);}
    });
    QTimer::singleShot(30000,&window,[&]{qCritical("Live conflict test timed out");app.exit(1);});
}
