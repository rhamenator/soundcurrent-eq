// SPDX-License-Identifier: GPL-3.0-only
// Opt-in regression through actual buttons, helper processes and error dialogs.
if (app.arguments().contains("--windows-audio-setup-test")) {
    if (app.arguments().size() != 4 && app.arguments().size() != 6) return 2;
    MainWindow window(false);
    window.show();
    const auto action = app.arguments()[2];
    const auto expected = app.arguments()[3];
    auto *button = window.findChild<QPushButton *>(action == "install" ? "audioDriverSetup" : "cableSettings");
    if (!button || !button->isEnabled()) return 3;
    QTimer watch;
    watch.setInterval(50);
    QObject::connect(&watch, &QTimer::timeout, &window, [&] {
        for (auto *widget : app.topLevelWidgets()) {
            auto *box = qobject_cast<QMessageBox *>(widget);
            if (!box || !box->isVisible()) continue;
            const auto text = box->text();
            if (!text.contains(expected, Qt::CaseInsensitive)) {
                qCritical("Unexpected setup result: dialog UTF-8=%s; expected UTF-8=%s", text.toUtf8().toHex().constData(), expected.toUtf8().toHex().constData());
                box->accept(); app.exit(4); return;
            }
            if (!window.isVisible() || !button->isEnabled()) {
                box->accept(); app.exit(5); return;
            }
            qInfo("PASS: action UTF-8=%s; expected UTF-8=%s; dialog UTF-8=%s; app remains open; controls usable", action.toUtf8().toHex().constData(), expected.toUtf8().toHex().constData(), text.toUtf8().toHex().constData());
            watch.stop(); box->accept();
            QTimer::singleShot(0, &app, [&] { app.exit(0); });
            return;
        }
    });
    watch.start();
    QTimer::singleShot(0, button, &QPushButton::click);
    QTimer::singleShot(30000, &app, [&] { qCritical("Setup regression timed out"); app.exit(6); });
    return app.exec();
}
