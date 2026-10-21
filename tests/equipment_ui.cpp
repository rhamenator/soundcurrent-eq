// SPDX-License-Identifier: GPL-3.0-only
#include "equipment_profiles.h"
#include "localization.h"
#include "equipment_display_text.h"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QUuid>
#include <cstdio>
using namespace soundcurrent::equipment;
namespace {
void require(bool ok, const char *message) { if (!ok) qFatal("%s", message); }
QDialog *dialog(const QString &title) {
    for (auto *w: QApplication::topLevelWidgets())
        if (w->isVisible() && w->windowTitle()==soundcurrent::i18n::text(title.toUtf8().constData())) return qobject_cast<QDialog *>(w);
    return nullptr;
}
QPushButton *button(QWidget *w, const QString &text) {
    for (auto *b:w->findChildren<QPushButton *>()) if(b->text()==soundcurrent::i18n::text(text.toUtf8().constData()))return b;
    qFatal("Missing button: %s",qPrintable(text));return nullptr;
}
void clickLater(QAbstractButton *b) { require(b,"Missing dialog action"); QTimer::singleShot(0,b,&QAbstractButton::click); }
void closeLater(QDialog *w) { QTimer::singleShot(0,w,&QDialog::reject); }
void setGain(QDialog *w, double value) {
    auto *table=w->findChild<QTableWidget *>();require(table && table->rowCount()>0,"No filter editor");
    auto *gain=qobject_cast<QDoubleSpinBox *>(table->cellWidget(0,2));require(gain,"No gain editor");gain->setValue(value);
}
}
int main(int argc,char **argv) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc,argv);
    QTemporaryDir temp;require(temp.isValid(),"No isolated profile folder");
    app.setOrganizationName("SoundCurrentUITests");app.setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QStandardPaths::setTestModeEnabled(true);
    soundcurrent::i18n::Runtime localization;
    localization.initialize(!app.arguments().contains("--language"));
    // A unique application name keeps every read/write away from real profiles.
    const auto path=libraryPath();require(loadLibrary().isEmpty(),"Test library not isolated");
    Profile fixture;fixture.kind="speaker";fixture.brand="UI fixture";fixture.family="Test";fixture.model="Saved profile";
    fixture.source="https://example.invalid/ui-test";fixture.conditions="Synthetic test only";fixture.filters={{1000,0,1}};
    QTimer drive;drive.setInterval(20);int phase=0;bool checkedCancel=false;
    QElapsedTimer elapsed,progressElapsed;elapsed.start();progressElapsed.start();QTimer phaseTrace;int lastPhase=-1;
    QObject::connect(&phaseTrace,&QTimer::timeout,&app,[&]{if(phase!=lastPhase){
        qInfo("Equipment fixture phase %d at %lld ms",phase,static_cast<long long>(elapsed.elapsed()));lastPhase=phase;progressElapsed.restart();}});
    phaseTrace.start(100);
    QObject::connect(&drive,&QTimer::timeout,&app,[&] {
        if(auto *box=qobject_cast<QMessageBox *>(dialog("Save modified profile?"))) {
            if(phase==1){require(box->button(QMessageBox::Cancel),"No cancel choice");phase=2;clickLater(box->button(QMessageBox::Cancel));}
            else if(phase==3){phase=4;clickLater(box->button(QMessageBox::Discard));}
            return;
        }
        if(auto *editor=dialog("Equipment profile editor")) {
            if(phase==0){
                require(editor->findChild<QLineEdit *>("profileSource")->maxLength()==2048,"Translated source label changed its field limit");
                require(editor->findChild<QLineEdit *>("profileConditions")->maxLength()==2000,"Translated conditions label changed its field limit");
                setGain(editor,3);phase=1;closeLater(editor);}
            else if(phase==2){checkedCancel=true;phase=3;closeLater(editor);}
        }
    });
    QTimer watchdog;QObject::connect(&watchdog,&QTimer::timeout,&app,[&]{if(progressElapsed.elapsed()<20000)return;for(auto *w:QApplication::topLevelWidgets())if(w->isVisible()){qWarning("Visible: %s / %s",w->metaObject()->className(),qPrintable(w->windowTitle()));if(auto *f=qobject_cast<QFileDialog *>(w))qWarning("Selected: %s",qPrintable(f->selectedFiles().join(";")));if(auto *b=qobject_cast<QMessageBox *>(w))qWarning("Message: %s",qPrintable(b->text()));w->grab().save(temp.filePath("timeout.png"));}qFatal("Equipment UI test made no phase progress for 20 seconds at phase %d",phase);});watchdog.start(100);
    drive.start();saveNewProfile(nullptr,fixture);drive.stop();
    require(phase==4 && checkedCancel && loadLibrary().isEmpty(),"Cancel/discard modified profile failed");
    phase=0;
    QObject::disconnect(&drive,nullptr,&app,nullptr);
    QObject::connect(&drive,&QTimer::timeout,&app,[&] {
        if(auto *editor=dialog("Equipment profile editor")) {
            if(phase==0){setGain(editor,2.5);phase=1;auto *box=editor->findChild<QDialogButtonBox *>();require(box,"No editor buttons");clickLater(box->button(QDialogButtonBox::Save));}
        }
    });
    drive.start();saveNewProfile(nullptr,fixture);drive.stop();
    auto saved=loadLibrary();require(saved.size()==1 && saved[0].custom && saved[0].filters[0].gainDb==2.5,"Save editor failed");
    const auto originalId=saved[0].id;
    QFile invalid(temp.filePath("invalid.json"));require(invalid.open(QIODevice::WriteOnly),"No malformed import fixture");invalid.write("{ broken");invalid.close();
    QFile valid(temp.filePath("valid.json"));require(valid.open(QIODevice::WriteOnly),"No valid import fixture");
    fixture.model="Imported profile";fixture.id="ui-import-fixture";valid.write(QJsonDocument(serialize(fixture)).toJson());valid.close();
    phase=0;bool applied=false,malformedRejected=false;
    QObject::disconnect(&drive,nullptr,&app,nullptr);
    QObject::connect(&drive,&QTimer::timeout,&app,[&] {
        for(auto *w:QApplication::topLevelWidgets()) {
            if(!w->isVisible())continue;
            if(auto *file=qobject_cast<QFileDialog *>(w)) {
                if(phase==1 || phase==4){const auto name=phase==1?invalid.fileName():valid.fileName();phase=phase==1?2:5;QTimer::singleShot(250,file,[file,name]{auto *input=file->findChild<QLineEdit *>("fileNameEdit");require(input,"No file selection input");input->setText(name);QMetaObject::invokeMethod(file,"accept",Qt::QueuedConnection);});return;}continue;
            }
            if(auto *box=qobject_cast<QMessageBox *>(w)) {
                if(phase==2){malformedRejected=true;require(loadLibrary().size()==1,"Malformed import changed library");phase=3;require(box->button(QMessageBox::Ok),"Malformed import warning has no OK button");clickLater(box->button(QMessageBox::Ok));}
                else if(phase==5){require(box->button(QMessageBox::Yes),"Import confirmation missing");phase=6;clickLater(box->button(QMessageBox::Yes));}
                else if(phase==7){phase=8;clickLater(box->button(QMessageBox::Yes));}
                return;
            }
        }
        auto *library=dialog("Equipment profiles — brand / family / model");if(!library)return;
        if(phase==0){require(bundledProfiles().size()>1000,"Bundled equipment missing");
            auto *kind=library->findChild<QComboBox *>("equipmentKindFilter");require(kind,"No equipment kind selector");
            require(kind->itemData(1).toString()=="speaker" && kind->itemData(2).toString()=="microphone" && kind->itemData(3).toString()=="amplifier","Localized kinds changed profile IDs");
            auto *subtype=library->findChild<QComboBox *>("equipmentSubtypeFilter");require(subtype,"No stable equipment subtype selector");
            const int bookshelf=subtype->findData("Bookshelf");require(bookshelf>=0,"Missing bookshelf taxonomy key");
            require(subtype->itemText(bookshelf)==soundcurrent::i18n::text("Bookshelf speaker"),"Built-in subtype caption is not localized");
            require(soundcurrent::i18n::equipmentTypeText("Custom %1 / 棚型")=="Custom %1 / 棚型","Custom subtype text was changed");
            subtype->setItemText(bookshelf,QString::fromUtf8("Bibliothèque / 棚型"));subtype->setCurrentIndex(bookshelf);
            require(subtype->currentData().toString()=="Bookshelf","Display caption changed subtype identity");
            kind->setCurrentIndex(1);require(subtype->currentData().toString()=="Bookshelf","Taxonomy refresh lost stable subtype selection");
            subtype->setCurrentIndex(0);kind->setCurrentIndex(2);kind->setCurrentIndex(0);phase=1;clickLater(button(library,"Import JSON"));}
        else if(phase==3){phase=4;clickLater(button(library,"Import JSON"));}
        else if(phase==6){auto profiles=loadLibrary();require(profiles.size()==2 && profiles[0].id==originalId,"Valid import overwrote existing profile");phase=7;clickLater(button(library,"Apply profile"));}
        else if(phase==8){require(applied,"Apply callback missing");phase=9;closeLater(library);}
    });
    drive.start();openLibrary(nullptr,[&](const Profile &p){require(p.model=="Imported profile","Applied wrong imported profile");applied=true;});drive.stop();
    require(phase==9 && applied && malformedRejected,"Profile import/apply UI failed");
    // Dedicated managers start on the requested route; bundled curves are protected.
    for (const QString kind : {QString("speaker"), QString("microphone"), QString("amplifier")}) {
        QObject::disconnect(&drive,nullptr,&app,nullptr);
        bool visited = false;
        QObject::connect(&drive,&QTimer::timeout,&app,[&] {
            auto *library=dialog("Equipment profiles — brand / family / model");
            if(!library || visited)return;
            visited=true;
            auto *filter=library->findChild<QComboBox *>("equipmentKindFilter");
            require(filter && filter->currentData().toString()==kind,"Manager opened on wrong equipment type");
            const auto screenshots=qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");
            if(!screenshots.isEmpty()){QDir().mkpath(screenshots);require(library->grab().save(QDir(screenshots).filePath("manager-"+kind+".png")),"Could not capture manager screenshot");}
            auto *list=library->findChild<QComboBox *>("equipmentProfileList");
            auto *remove=library->findChild<QPushButton *>("deleteEquipmentProfile");
            require(list && remove,"Management controls missing");
            filter->setCurrentIndex(0);list->setCurrentIndex(0);
            require(!remove->isEnabled(),"Bundled profile can be deleted");
            closeLater(library);
        });
        drive.start();openLibrary(nullptr,[](const Profile &){},kind);drive.stop();
        require(visited,"Typed profile manager was not exercised");
    }
    // Cancellation is non-destructive; confirmed deletion affects only the chosen saved copy.
    QObject::disconnect(&drive,nullptr,&app,nullptr);phase=0;
    const auto deleteId=loadLibrary().front().id;
    const auto preservedId=loadLibrary().back().id;
    QObject::connect(&drive,&QTimer::timeout,&app,[&] {
        for(auto *w:QApplication::topLevelWidgets())if(auto *box=qobject_cast<QMessageBox *>(w)) {
            if(!box->isVisible())continue;
            if(phase==1){phase=2;clickLater(box->button(QMessageBox::No));return;}
            if(phase==3){phase=4;clickLater(box->button(QMessageBox::Yes));return;}
        }
        auto *library=dialog("Equipment profiles — brand / family / model");
        if(!library)return;
        auto *list=library->findChild<QComboBox *>("equipmentProfileList");
        auto *remove=library->findChild<QPushButton *>("deleteEquipmentProfile");
        if(phase==0){
            const auto combined=bundledProfiles()+loadLibrary();
            for(int i=0;i<list->count();++i)if(combined[list->itemData(i).toInt()].id==deleteId)list->setCurrentIndex(i);
            require(remove->isEnabled(),"Saved profile deletion unavailable");phase=1;clickLater(remove);
        }else if(phase==2){require(loadLibrary().size()==2,"Cancelled deletion changed library");phase=3;clickLater(remove);}
        else if(phase==4){const auto remaining=loadLibrary();require(remaining.size()==1 && remaining.front().id==preservedId,"Deletion affected another saved profile");phase=5;closeLater(library);}
    });
    drive.start();openLibrary(nullptr,[](const Profile &) {},"speaker");drive.stop();
    require(phase==5,"Saved-profile deletion workflow incomplete");
    for (const QString kind : {QString("speaker"), QString("microphone"), QString("amplifier")}) {
        const int before=loadLibrary().size();
        QObject::disconnect(&drive,nullptr,&app,nullptr);phase=0;
        QObject::connect(&drive,&QTimer::timeout,&app,[&] {
            if(phase==1)for(auto *w:QApplication::topLevelWidgets()) {
                auto *selection=qobject_cast<QDialog *>(w);
                if(!selection || !selection->isVisible())continue;
                auto *type=selection->findChild<QComboBox *>("equipmentCreationType", Qt::FindDirectChildrenOnly);
                if(type){require(type->currentData().toString()==kind,"Create profile defaulted to wrong equipment type");phase=2;QTimer::singleShot(0,selection,&QDialog::accept);return;}
            }
            if(phase==2)if(auto *editor=dialog("Equipment profile editor")) {
                setGain(editor,0.5);phase=3;clickLater(editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save));return;
            }
            if(auto *library=dialog("Equipment profiles — brand / family / model")) {
                if(phase==0){phase=1;clickLater(button(library,"Create profile"));}
                else if(phase==3){const auto profiles=loadLibrary();require(profiles.size()==before+1 && profiles.back().kind==kind,"Created profile was not saved to requested route");phase=4;closeLater(library);}
            }
        });
        drive.start();openLibrary(nullptr,[](const Profile &) {},kind);drive.stop();
        require(phase==4,"Creating an equipment profile through manager failed");
    }
    // Imported copies may keep a bundled ID: identity collisions must not unlock originals.
    auto collisionLibrary=loadLibrary();collisionLibrary.append(bundledProfiles().front());saveLibrary(collisionLibrary);
    QObject::disconnect(&drive,nullptr,&app,nullptr);bool collisionChecked=false;
    QObject::connect(&drive,&QTimer::timeout,&app,[&] {
        auto *library=dialog("Equipment profiles — brand / family / model");if(!library || collisionChecked)return;
        auto *list=library->findChild<QComboBox *>("equipmentProfileList");
        auto *remove=library->findChild<QPushButton *>("deleteEquipmentProfile");
        list->setCurrentIndex(0);require(!remove->isEnabled(),"Imported ID collision unlocked bundled deletion");
        list->setCurrentIndex(list->count()-1);require(remove->isEnabled(),"Imported copy cannot be managed");
        collisionChecked=true;closeLater(library);
    });
    drive.start();openLibrary(nullptr,[](const Profile &) {});drive.stop();require(collisionChecked,"Collision protection was not exercised");
    QFile::remove(path);
    std::puts("PASS: profile editor change, Cancel/Discard prompt, save, malformed/valid JSON import, preservation and Apply");
    return 0;
}
