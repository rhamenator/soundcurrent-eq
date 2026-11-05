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
#include <QLabel>
#include <QCheckBox>
#include <QListWidget>
#include <QScrollArea>
#include <QScrollBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QSlider>
#include <QGroupBox>
#include <QScreen>
#include <QUuid>
#include <cstdio>
#include <cmath>
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
    std::optional<Profile> previewed;int previewRestores=0;bool failPreview=true;
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
                auto *graph=editor->findChild<QWidget *>("profileCorrectionGraph");
                auto *advanced=editor->findChild<QGroupBox *>("profileAdvanced");
                auto *table=editor->findChild<QTableWidget *>();
                auto *preview=editor->findChild<QPushButton *>("previewProfile");
                require(graph && advanced && !advanced->isChecked() && table && !table->isVisible(),"Graph editor did not start in simple mode");
                require(editor->height()<=editor->screen()->availableGeometry().height(),"Editor exceeded display height");
                auto *box=editor->findChild<QDialogButtonBox *>();
                for(auto role:{QDialogButtonBox::Save,QDialogButtonBox::Cancel}){
                    auto *action=box->button(role);require(action->isVisible() && editor->rect().contains(QRect(action->mapTo(editor,QPoint()),action->size())),"Editor footer requires scrolling");}
                require(preview && preview->isEnabled(),"Editor cannot audition through preview callback");
                require(!previewed,"Opening editor changed preview");
                button(editor,"Add point")->click();require(table->rowCount()==2,"Add point failed");
                editor->findChild<QSlider *>("profilePointGain")->setValue(30);
                const auto oldFrequency=qobject_cast<QDoubleSpinBox *>(table->cellWidget(1,1))->value();
                preview->click();require(!preview->isChecked() && !previewed && previewRestores==1,"Failed preview left a temporary correction active");
                preview->click();require(previewed && previewed->filters.size()==2 && previewed->filters[1].gainDb==3,"Preview did not use drawn correction");
                const QRectF area(45,15,graph->width()-90,graph->height()-40);
                auto point=[&](double f,double gain){return QPointF(area.left()+std::log(f/20.)/std::log(1000.)*area.width(),area.center().y()-gain/24.*area.height());};
                auto send=[&](QEvent::Type type,QPointF position,Qt::MouseButton pressed,Qt::MouseButtons held){QMouseEvent event(type,position,graph->mapToGlobal(position.toPoint()),pressed,held,Qt::NoModifier);QCoreApplication::sendEvent(graph,&event);};
                send(QEvent::MouseButtonPress,point(oldFrequency,3),Qt::LeftButton,Qt::LeftButton);
                send(QEvent::MouseMove,point(2000,2),Qt::NoButton,Qt::LeftButton);
                send(QEvent::MouseButtonRelease,point(2000,2),Qt::LeftButton,Qt::NoButton);
                require(previewed && std::abs(previewed->filters[1].frequency-2000)<.01 && std::abs(previewed->filters[1].gainDb-2)<.01,"Graph drag did not update preview immediately");
                button(editor,"Undo")->click();require(std::abs(previewed->filters[1].frequency-oldFrequency)<.0001 && previewed->filters[1].gainDb==3,"One drag did not undo as one action");
                const auto beforeKey=previewed->filters[1].gainDb;
                QKeyEvent key(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier);QCoreApplication::sendEvent(graph,&key);
                require(previewed->filters[1].gainDb<beforeKey,"Keyboard graph edit did not update preview");button(editor,"Undo")->click();
                send(QEvent::MouseButtonDblClick,point(5000,1),Qt::LeftButton,Qt::LeftButton);
                require(table->rowCount()==3 && previewed->filters.size()==3,"Double-click did not add a curve point");
                QKeyEvent erase(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);QCoreApplication::sendEvent(graph,&erase);
                require(table->rowCount()==2,"Keyboard Delete did not remove a point");
                while(table->rowCount()<32)button(editor,"Add point")->click();
                require(!button(editor,"Add point")->isEnabled(),"Editor exceeds the 32-point bound");
                while(table->rowCount()>1)button(editor,"Remove point")->click();
                require(!button(editor,"Remove point")->isEnabled(),"Editor allowed an empty correction");
                advanced->setChecked(true);require(table->isVisible(),"Advanced filter controls are inaccessible");advanced->setChecked(false);
                const auto beforeWheel=editor->findChild<QSlider *>("profilePointGain")->value();
                QWheelEvent wheel(QPointF(5,5),QPointF(5,5),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
                QCoreApplication::sendEvent(editor->findChild<QSlider *>("profilePointGain"),&wheel);
                require(editor->findChild<QSlider *>("profilePointGain")->value()==beforeWheel,"Scrolling changed profile slider");
                preview->click();require(!previewed && previewRestores==2,"Turning preview off did not restore baseline");preview->click();
                const auto captures=qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");
                if(!captures.isEmpty()){QDir().mkpath(captures);editor->grab().save(QDir(captures).filePath("graph-editor.png"));}
                phase=1;closeLater(editor);}
            else if(phase==2){require(bool(previewed),"Canceling close prompt stopped an ongoing preview");checkedCancel=true;phase=3;closeLater(editor);}
        }
    });
    QTimer watchdog;QObject::connect(&watchdog,&QTimer::timeout,&app,[&]{if(progressElapsed.elapsed()<20000)return;for(auto *w:QApplication::topLevelWidgets())if(w->isVisible()){qWarning("Visible: %s / %s",w->metaObject()->className(),qPrintable(w->windowTitle()));if(auto *f=qobject_cast<QFileDialog *>(w))qWarning("Selected: %s",qPrintable(f->selectedFiles().join(";")));if(auto *b=qobject_cast<QMessageBox *>(w))qWarning("Message: %s",qPrintable(b->text()));w->grab().save(temp.filePath("timeout.png"));}qFatal("Equipment UI test made no phase progress for 20 seconds at phase %d",phase);});watchdog.start(100);
    drive.start();saveNewProfile(nullptr,fixture,[&](const std::optional<Profile> &p){previewed=p;if(!p)++previewRestores;if(p && failPreview){failPreview=false;return false;}return true;});drive.stop();
    require(!previewed && previewRestores==3,"Discarding editor did not restore previous correction");
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
    fixture.model="Imported profile";fixture.id="ui-import-fixture";
    QByteArray importBytes=QJsonDocument(serialize(fixture)).toJson();
    const auto realFixture=qEnvironmentVariable("SOUNDCURRENT_PROFILE_IMPORT_FIXTURE");
    if(!realFixture.isEmpty()){QFile real(realFixture);require(real.open(QIODevice::ReadOnly),"Cannot read real published import fixture");importBytes=real.readAll();}
    const auto expectedImportedModel=parse(importBytes).model;
    valid.write(importBytes);valid.close();
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
        else if(phase==3){library->findChild<QCheckBox *>("customProfilesOnly")->setChecked(true);phase=4;clickLater(button(library,"Import JSON"));}
        else if(phase==6){require(!applied,"Importing or saving implicitly changed audio");auto profiles=loadLibrary();require(profiles.size()==2 && profiles[0].id==originalId,"Valid import overwrote existing profile");
            auto *saved=library->findChild<QListWidget *>("savedCustomProfiles");
            require(saved && saved->currentItem(),"Imported custom profile is not visibly selected");
            phase=7;QTimer::singleShot(0,saved,[saved]{QMetaObject::invokeMethod(saved,"itemClicked",Qt::DirectConnection,Q_ARG(QListWidgetItem *,saved->currentItem()));});}
        else if(phase==7){if(!applied)return;applied=false;phase=8;clickLater(button(library,"Apply profile"));}
        else if(phase==8){if(!applied)return;phase=9;}
        else if(phase==9){
            auto *feedback=library->findChild<QLabel *>("appliedEquipmentProfile");
            require(feedback && feedback->text().contains(expectedImportedModel),"Applied import has no visible confirmation");

            require(feedback->isVisible() && library->rect().contains(QRect(feedback->mapTo(library,QPoint()),feedback->size())),
                    "Applied confirmation requires scrolling or is clipped");
            auto *body=library->findChild<QScrollArea *>();
            if(body && body->horizontalScrollBar()->maximum()>0) {
                qWarning("Profile viewport width %d; content width %d",body->viewport()->width(),body->widget()->width());
                for(auto *child:body->widget()->findChildren<QWidget *>())
                    if(child->minimumSizeHint().width()>80) qWarning("%s: minimum %d, width %d",child->metaObject()->className(),child->minimumSizeHint().width(),child->width());
                const auto debugCaptures=qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");
                if(!debugCaptures.isEmpty()){QDir().mkpath(debugCaptures);library->grab().save(QDir(debugCaptures).filePath("horizontal-overflow.png"));}
            }
            require(body && body->horizontalScrollBar()->maximum()==0,"Profile manager clips content horizontally");
            auto *closeBox=library->findChild<QDialogButtonBox *>();
            auto *closeButton=closeBox ? closeBox->button(QDialogButtonBox::Close) : nullptr;
            auto *applyButton=button(library,"Apply profile");
            require(applyButton && applyButton->isVisible() && library->rect().contains(QRect(applyButton->mapTo(library,QPoint()),applyButton->size())),
                    "Apply button requires scrolling or is clipped");
            require(!applyButton->toolTip().isEmpty(),"Apply has no explanatory tooltip");
            require(closeButton && closeButton->isVisible() &&
                    library->rect().contains(QRect(closeButton->mapTo(library,QPoint()),closeButton->size())),
                    "Close button requires scrolling or is clipped");
            const auto captures=qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");
            if(!captures.isEmpty()){
                QDir().mkpath(captures);
                require(library->grab().save(QDir(captures).filePath("import-applied.png")),"Could not capture imported profile confirmation");
            }
            phase=10;closeLater(library);}
    });
    drive.start();openLibrary(nullptr,[&](const Profile &p){require(p.model==expectedImportedModel,"Applied wrong imported profile");applied=true;});drive.stop();
    require(phase==10 && applied && malformedRejected,"Profile import/apply UI failed");
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
            if(!screenshots.isEmpty()){
                QDir().mkpath(screenshots);
                auto *choices=library->findChild<QComboBox *>("equipmentProfileList");const auto combined=bundledProfiles()+loadLibrary();
                for(int row=1;row<choices->count();++row){const auto &profile=combined[choices->itemData(row).toInt()];
                    if((kind=="microphone" && profile.brand=="Shure" && profile.equipmentType=="Lavalier microphone") ||
                       (kind=="amplifier" && profile.brand=="Fosi Audio") ||
                       (kind=="speaker" && profile.brand=="Sony")){choices->setCurrentIndex(row);break;}}
                QCoreApplication::processEvents();
                require(library->grab().save(QDir(screenshots).filePath("manager-"+kind+".png")),"Could not capture manager screenshot");
            }
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
    // Saved custom equipment has a visible, selectable management list.
    QObject::disconnect(&drive,nullptr,&app,nullptr);bool customChecked=false;
    QObject::connect(&drive,&QTimer::timeout,&app,[&] {
        auto *manager=dialog("Equipment profiles — brand / family / model");if(!manager || customChecked)return;
        auto *only=manager->findChild<QCheckBox *>("customProfilesOnly");
        auto *saved=manager->findChild<QListWidget *>("savedCustomProfiles");
        require(only && saved,"Missing custom profile management list");only->setChecked(true);
        require(saved->count()==loadLibrary().size() && saved->isVisible(),"Saved profiles not available in custom list");
        saved->setCurrentRow(saved->count()-1);
        require(manager->findChild<QComboBox *>("equipmentProfileList")->currentIndex()>0,"Custom list did not select profile");
        auto *choices=manager->findChild<QComboBox *>("equipmentProfileList");
        choices->setCurrentIndex(saved->item(0)->data(Qt::UserRole).toInt());
        require(saved->currentRow()==0,"Dropdown changed action target without updating highlighted custom entry");
        saved->setCurrentRow(saved->count()-1);
        require(choices->currentIndex()==saved->currentItem()->data(Qt::UserRole).toInt(),"Custom selection failed to restore action target");
        require(manager->findChild<QPushButton *>("deleteEquipmentProfile")->isEnabled(),"Custom list cannot manage saved profile");
        customChecked=true;
        const auto captures=qEnvironmentVariable("SOUNDCURRENT_UI_SCREENSHOT_DIR");
        if(!captures.isEmpty()){QCoreApplication::processEvents();require(manager->grab().save(QDir(captures).filePath("custom-library.png")),"Could not capture custom library");}
        closeLater(manager);
    });
    drive.start();openLibrary(nullptr,[](const Profile &) {},"speaker");drive.stop();require(customChecked,"Custom profile list not tested");
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
            for(int i=0;i<list->count();++i)if(list->itemData(i).toInt()>=0 && combined[list->itemData(i).toInt()].id==deleteId)list->setCurrentIndex(i);
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
        list->setCurrentIndex(1);require(!remove->isEnabled(),"Imported ID collision unlocked bundled deletion");
        list->setCurrentIndex(list->count()-1);require(remove->isEnabled(),"Imported copy cannot be managed");
        collisionChecked=true;closeLater(library);
    });
    drive.start();openLibrary(nullptr,[](const Profile &) {});drive.stop();require(collisionChecked,"Collision protection was not exercised");
    // Immediate selection, route-specific None and refresh safety are an audio contract.
    QObject::disconnect(&drive,nullptr,&app,nullptr);
    int notifications=0; bool immediateChecked=false;
    QObject::connect(&drive,&QTimer::timeout,&app,[&] {
        auto *library=dialog("Equipment profiles — brand / family / model");
        if(!library || immediateChecked)return;
        auto *list=library->findChild<QComboBox *>("equipmentProfileList");
        auto *kind=library->findChild<QComboBox *>("equipmentKindFilter");
        require(notifications==0 && list->currentData().toInt()==-1,"Opening manager changed audio or chose equipment by default");
        QWheelEvent wheel(QPointF(5,5),QPointF(5,5),QPoint(),QPoint(0,-120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QCoreApplication::sendEvent(list,&wheel);
        require(list->currentIndex()==0 && notifications==0,"Scrolling accidentally selected and applied an equipment profile");
        // Choosing the already displayed None row still clears an existing route.
        QMetaObject::invokeMethod(list,"activated",Qt::DirectConnection,Q_ARG(int,0));
        require(notifications==1,"Initial None activation did not clear route");
        kind->setCurrentIndex(kind->findData("microphone"));
        kind->setCurrentIndex(kind->findData("amplifier"));
        require(notifications==1,"Taxonomy refresh changed audio");
        list->setCurrentIndex(1);
        require(notifications==2 && !QApplication::activeModalWidget()->inherits("QMessageBox"),"Profile did not apply immediately without confirmation");
        list->setCurrentIndex(0);
        require(notifications==3,"None did not immediately clear selected equipment route");
        immediateChecked=true;closeLater(library);
    });
    drive.start();openLibrary(nullptr,[&](const Profile &p) {
        ++notifications;
        if(notifications==1 || notifications==3)require(p.id.isEmpty() && p.kind=="amplifier","None cleared wrong equipment route");
        else require(!p.id.isEmpty() && !p.filters.isEmpty(),"Correction selection had no filters");
    },"amplifier");drive.stop();require(immediateChecked,"Immediate profile selection workflow incomplete");
    QFile::remove(path);
    std::puts("PASS: profile editor change, Cancel/Discard prompt, save, malformed/valid JSON import, preservation and Apply");
    return 0;
}
