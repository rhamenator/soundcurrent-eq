// SPDX-License-Identifier: GPL-3.0-only
#include "equipment_profiles.h"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
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
        if (w->isVisible() && w->windowTitle()==title) return qobject_cast<QDialog *>(w);
    return nullptr;
}
QPushButton *button(QWidget *w, const QString &text) {
    for (auto *b:w->findChildren<QPushButton *>()) if(b->text()==text)return b;
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
    // A unique application name keeps every read/write away from real profiles.
    const auto path=libraryPath();require(loadLibrary().isEmpty(),"Test library not isolated");
    Profile fixture;fixture.kind="speaker";fixture.brand="UI fixture";fixture.family="Test";fixture.model="Saved profile";
    fixture.source="https://example.invalid/ui-test";fixture.conditions="Synthetic test only";fixture.filters={{1000,0,1}};
    QTimer drive;drive.setInterval(20);int phase=0;bool checkedCancel=false;
    QObject::connect(&drive,&QTimer::timeout,&app,[&] {
        if(auto *box=qobject_cast<QMessageBox *>(dialog("Save modified profile?"))) {
            if(phase==1){require(box->button(QMessageBox::Cancel),"No cancel choice");phase=2;clickLater(box->button(QMessageBox::Cancel));}
            else if(phase==3){phase=4;clickLater(box->button(QMessageBox::Discard));}
            return;
        }
        if(auto *editor=dialog("Equipment profile editor")) {
            if(phase==0){setGain(editor,3);phase=1;closeLater(editor);}
            else if(phase==2){checkedCancel=true;phase=3;closeLater(editor);}
        }
    });
    QTimer::singleShot(20000,&app,[&]{for(auto *w:QApplication::topLevelWidgets())if(w->isVisible()){qWarning("Visible: %s / %s",w->metaObject()->className(),qPrintable(w->windowTitle()));if(auto *f=qobject_cast<QFileDialog *>(w))qWarning("Selected: %s",qPrintable(f->selectedFiles().join(";")));if(auto *b=qobject_cast<QMessageBox *>(w))qWarning("Message: %s",qPrintable(b->text()));w->grab().save(temp.filePath("timeout.png"));}qFatal("Equipment UI test timed out at phase %d",phase);});
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
                if(phase==2){malformedRejected=true;require(loadLibrary().size()==1,"Malformed import changed library");phase=3;QTimer::singleShot(0,box,&QMessageBox::accept);}
                else if(phase==5){require(box->button(QMessageBox::Yes),"Import confirmation missing");phase=6;clickLater(box->button(QMessageBox::Yes));}
                else if(phase==7){phase=8;clickLater(box->button(QMessageBox::Yes));}
                return;
            }
        }
        auto *library=dialog("Equipment profiles — brand / family / model");if(!library)return;
        if(phase==0){require(bundledProfiles().size()>1000,"Bundled equipment missing");phase=1;clickLater(button(library,"Import JSON"));}
        else if(phase==3){phase=4;clickLater(button(library,"Import JSON"));}
        else if(phase==6){auto profiles=loadLibrary();require(profiles.size()==2 && profiles[0].id==originalId,"Valid import overwrote existing profile");phase=7;clickLater(button(library,"Apply profile"));}
        else if(phase==8){require(applied,"Apply callback missing");phase=9;closeLater(library);}
    });
    drive.start();openLibrary(nullptr,[&](const Profile &p){require(p.model=="Imported profile","Applied wrong imported profile");applied=true;});drive.stop();
    require(phase==9 && applied && malformedRejected,"Profile import/apply UI failed");
    QFile::remove(path);
    std::puts("PASS: profile editor change, Cancel/Discard prompt, save, malformed/valid JSON import, preservation and Apply");
    return 0;
}
