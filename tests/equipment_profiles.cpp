// SPDX-License-Identifier: GPL-3.0-only
#include "equipment_profiles.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cmath>
#include <iostream>
using namespace soundcurrent::equipment;
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir dir;
    check(dir.isValid(), "temporary directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    QCoreApplication::setOrganizationName("SoundCurrentProfileTest");
    QCoreApplication::setApplicationName("Equipment");
    try {
        Profile original;
        original.id = "test-source";
        original.kind = "microphone";
        original.brand = "Test";
        original.family = "Measured";
        original.model = "Example";
        original.source = "https://example.org/measurement";
        original.conditions = "Synthetic test, on axis";
        original.filters.append({1000, -2, 1});
        auto bytes = QJsonDocument(serialize(original)).toJson();
        auto round = parse(bytes);
        check(round.filters.size() == 1 && round.kind == "microphone", "round trip");
        auto detailed = original;
        detailed.filters.fill(original.filters.front(), 32);
        auto detailedJson = serialize(detailed);
        check(detailedJson.value("schema").toInt() == 3, "extended schema");
        check(parse(QJsonDocument(detailedJson).toJson()).filters.size() == 32, "32-filter round trip");
        auto oldSchema = detailedJson;
        oldSchema["schema"] = 2;
        bool incompatible = false;
        try { parse(QJsonDocument(oldSchema).toJson()); }
        catch (const std::exception &) { incompatible = true; }
        check(incompatible, "schema 2 must retain its 16-filter limit");
        detailed.filters.append(original.filters.front());
        bool tooMany = false;
        try { parse(QJsonDocument(serialize(detailed)).toJson()); }
        catch (const std::exception &) { tooMany = true; }
        check(tooMany, "33 filters rejected");
        int rejected = 0;
        auto reject = [&](QJsonObject o) {
            try {
                parse(QJsonDocument(o).toJson());
            } catch (const std::exception &) {
                ++rejected;
                return;
            }
            throw std::runtime_error("invalid profile admitted");
        };
        auto o = serialize(original);
        o["kind"] = "invalid";
        reject(o);
        o = serialize(original);
        o["brand"] = "";
        reject(o);
        o = serialize(original);
        o["measurementSource"] = "file:///etc/passwd";
        reject(o);
        o = serialize(original);
        o["filters"] = QJsonArray{};
        reject(o);
        o = serialize(original);
        o["filters"] = QJsonArray{QJsonObject{{"type", "PK"}, {"frequency", 1000}, {"gain", 20}, {"q", 1}}};
        reject(o);
        o = serialize(original);
        o["response"] = QJsonArray{QJsonArray{1000, 0}, QJsonArray{100, 1}};
        reject(o);
        try {
            parse(QByteArray(1024 * 1024 + 1, 'x'));
            throw std::runtime_error("oversize admitted");
        } catch (const std::runtime_error &e) {
            check(std::string(e.what()) != "oversize admitted", "oversize");
        }
        QVector<Point> points;
        for (int i = 0; i < 128; ++i) {
            double f = 20 * std::pow(1000., i / 127.);
            points.append({f, soundcurrent::filterResponseDb({2000, 3, 1}, 48000, f)});
        }
        auto fitted = fitResponse(points);
        double before = 0, after = 0;
        for (auto v : points) {
            double d = v.db;
            before += d * d;
            for (auto b : fitted)
                d += soundcurrent::filterResponseDb(b, 48000, v.frequency);
            after += d * d;
        }
        check(after < before * .2, "correction sign / fit quality");
        auto bundle = bundledProfiles();
        check(bundle.size() >= 1000, "broad published catalog");
        for (const auto &p : bundle)
            parse(QJsonDocument(serialize(p)).toJson());
        int microphones=0, amplifiers=0, lavaliers=0;
        for (const auto &p : bundle) {
            if (p.kind=="microphone") { ++microphones; if(p.equipmentType=="Lavalier microphone")++lavaliers; }
            if (p.kind=="amplifier") ++amplifiers;
            if (!p.id.startsWith("measured-") && !p.id.startsWith("published-graph-"))continue;
            check(p.response.size()>=2,"Published correction has no measured response");
            double before=0, after=0;
            for(const auto &point:p.response) {
                double correction=0;
                for(const auto &b:p.filters)correction+=soundcurrent::filterResponseDb(b,48000,point.frequency);
                before+=point.db*point.db;after+=std::pow(point.db+correction,2);
            }
            check(after<before*.85,"Published correction does not improve its stated measurement");
            for(int i=0;i<=2048;++i) {
                double db=0;for(const auto &b:p.filters)db+=soundcurrent::filterResponseDb(b,48000,20*std::pow(1000.,i/2048.));
                check(std::isfinite(db) && std::abs(db)<=6.001,"Composite published correction exceeds gain cap");
            }
        }
        check(microphones>=40 && amplifiers>=15 && lavaliers>=4,"Broad measured microphone/amplifier/lavalier coverage missing");
        saveLibrary({original});
        check(loadLibrary().size() == 1, "atomic save/reopen");
        // Exercise real modal editor: change gain, close, choose Save in dirty prompt.
        QTimer::singleShot(0, [&] {
            auto *library = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            check(library != nullptr, "library dialog");
            if (app.arguments().contains("--screenshots"))
                library->grab().save("/tmp/sc-equipment-library.png");
            // A profile change is now intentional and immediate; merely opening is not.
            library->findChild<QComboBox *>("equipmentProfileList")->setCurrentIndex(1);
            for (auto *b : library->findChildren<QPushButton *>())
                if (b->text() == "Edit / save copy") {
                    QTimer::singleShot(0, [&] {
                        auto *editor = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                        check(editor && editor->windowTitle() == "Equipment profile editor", "editor dialog");
                        if (app.arguments().contains("--screenshots"))
                            editor->grab().save("/tmp/sc-equipment-editor.png");
                        auto spins = editor->findChildren<QDoubleSpinBox *>();
                        check(!spins.isEmpty(), "filter controls");
                        spins[1]->setValue(-1.5);
                        QTimer::singleShot(0, [] {
                            auto *prompt = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                            check(prompt && prompt->windowTitle() == "Save modified profile?",
                                  "dirty save prompt");
                            prompt->button(QMessageBox::Save)->click();
                        });
                        editor->reject();
                    });
                    b->click();
                    break;
                }
            library->reject();
        });
        openLibrary(nullptr,
                    [](const Profile &) {});
        auto saved = loadLibrary();
        check(saved.size() == 2 && saved[0].id == "test-source" && !saved[0].custom && saved[1].custom &&
                  saved[1].id != "test-source" && saved[1].filters[0].gainDb == -1.5,
              "save custom copy preserves reference");
        std::cout << "Equipment: " << bundle.size()
                  << " bundled profiles; parser limits, inverse fit, save/reopen and dirty-editor save copy "
                     "passed ("
                  << rejected << " invalid cases).\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
