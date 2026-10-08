#include "localization.h"
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QGroupBox>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <memory>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QCheckBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QVersionNumber>
#include <functional>
namespace soundcurrent {
inline QVersionNumber updateVersion(const QString &tag) {
    const auto match=QRegularExpression("(?:^|[-v])(\\d{1,6}\\.\\d{1,6}\\.\\d{1,6})(?:$|[-+])").match(tag);
    return match.hasMatch()?QVersionNumber::fromString(match.captured(1)):QVersionNumber();
}
inline bool newerUpdate(const QString &candidate,const QString &current) {
    const auto a=updateVersion(candidate),b=updateVersion(current);return !a.isNull()&&!b.isNull()&&QVersionNumber::compare(a,b)>0;
}
inline QPair<QString,QString> downloadedUpdate(const QString &repo,const QString &current,const QString &folder) {
    QString newest=current,file;
#ifdef Q_OS_WIN
    const QString prefix="SoundCurrent-"+(repo=="soundcurrent-eq"?QString("EQ"):QString("Studio"))+"-";
    const QRegularExpression pattern("^"+prefix+"(\\d+\\.\\d+\\.\\d+)-windows-x64-setup\\.exe$");
    const QStringList glob{prefix+"*-windows-x64-setup.exe"};
#else
    const QRegularExpression pattern("^"+repo+"_(\\d+\\.\\d+\\.\\d+)_amd64\\.deb$");
    const QStringList glob{repo+"_*_amd64.deb"};
#endif
    for(const auto &entry:QDir(folder).entryInfoList(glob,QDir::Files|QDir::Readable)) {
        const auto match=pattern.match(entry.fileName());
        if(match.hasMatch()&&newerUpdate(match.captured(1),newest)){newest=match.captured(1);file=entry.fileName();}
    }
    return {newest,file};
}
inline QString publishedUpdate(const QJsonArray &releases,const QString &repo,const QString &current,bool previews) {
    QString newest=current;
    for(const auto &value:releases) {
        const auto o=value.toObject();const QUrl url(o.value("html_url").toString());
        if(o.value("draft").toBool() || (o.value("prerelease").toBool()&&!previews) || url.scheme()!="https" || url.host()!="github.com" || !url.path().startsWith("/rhamenator/"+repo+"/releases/"))continue;
        const auto tag=o.value("tag_name").toString();if(newerUpdate(tag,newest))newest=updateVersion(tag).toString();
    }
    return newest;
}
class UpdatePanel : public QGroupBox {
public:
    UpdatePanel(QString repo,QString title,QString version,bool active,QWidget *parent=nullptr)
        :QGroupBox(SC_TR("Application updates"),parent),repo_(std::move(repo)),title_(std::move(title)),version_(std::move(version)),network_(this) {
        auto *layout=new QVBoxLayout(this);layout->addWidget(new QLabel(SC_TR("Installed version: %1").arg(version_)));
        auto *help=new QLabel(SC_TR("Install new packages over this version — no uninstall needed. Presets and profiles are kept. Save your work, use Quit (closing the window keeps it running), install the update, then reopen."));help->setWordWrap(true);layout->addWidget(help);
        enabled_=new QCheckBox(SC_TR("Remind me when updates are available or a restart is needed"));enabled_->setChecked(QSettings().value("updates/reminders",true).toBool());layout->addWidget(enabled_);
        preview_=new QCheckBox(SC_TR("Include preview releases"));preview_->setChecked(QSettings().value("updates/previews",true).toBool());layout->addWidget(preview_);
        status_=new QLabel(SC_TR("Checks published releases and downloaded installers. No update is installed automatically."));status_->setWordWrap(true);status_->setTextFormat(Qt::PlainText);layout->addWidget(status_);
        auto *row=new QHBoxLayout;auto *check=new QPushButton(SC_TR("Check for updates"));auto *download=new QPushButton(SC_TR("Open release downloads"));auto *folder=new QPushButton(SC_TR("Open update folder"));auto *choose=new QPushButton(SC_TR("Choose update folder…"));row->addWidget(check);row->addWidget(download);row->addWidget(folder);row->addWidget(choose);layout->addLayout(row);
        const auto downloads=QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        const auto suggested=downloads+"/SoundCurrent-Updates";
        folder_=QSettings().value("updates/folder",QFileInfo::exists(suggested)?suggested:downloads).toString();
        connect(check,&QPushButton::clicked,this,[this]{checkUpdates(true);});
        connect(download,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl("https://github.com/rhamenator/"+repo_+"/releases"));});
        connect(folder,&QPushButton::clicked,this,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(folder_));});
        connect(choose,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getExistingDirectory(this,SC_TR("Update download folder"),folder_);if(path.isEmpty())return;folder_=path;QSettings().setValue("updates/folder",path);checkLocal();});
        connect(enabled_,&QCheckBox::toggled,this,[](bool on){QSettings().setValue("updates/reminders",on);});
        connect(preview_,&QCheckBox::toggled,this,[](bool on){QSettings().setValue("updates/previews",on);});
        const QFileInfo binary(QCoreApplication::applicationFilePath());size_=binary.size();modified_=binary.lastModified();
        if(active){connect(&timer_,&QTimer::timeout,this,[this]{tick();});timer_.start(30000);QTimer::singleShot(1500,this,[this]{tick();});}
    }
    std::function<void(const QString &)> onReminder;
private:
    void remind(const QString &message){status_->setText(message);if(enabled_->isChecked() && message!=lastReminder_){lastReminder_=message;if(onReminder)onReminder(message);}}
    void checkLocal(){
        const auto candidate=downloadedUpdate(repo_,version_,folder_);const auto newest=candidate.first,file=candidate.second;
        if(!file.isEmpty())remind(SC_TR("Update %1 is downloaded: %2. Quit, install over the existing app, then reopen.").arg(newest,file));
    }
    void tick(){
        const QFileInfo binary(QCoreApplication::applicationFilePath());
        if(binary.size()!=size_ || binary.lastModified()!=modified_){remind(SC_TR("An application update was installed. Use Quit and reopen to load it; closing this window keeps the old version running."));return;}
        if(!enabled_->isChecked())return;
        checkLocal();
        const auto last=QSettings().value("updates/lastCheck").toDateTime();
        if(!last.isValid() || last.secsTo(QDateTime::currentDateTimeUtc())>=86400)checkUpdates(false);
    }
    void checkUpdates(bool manual){
        if(busy_)return;
        busy_=true;checkLocal();
        if(manual)status_->setText(SC_TR("Checking for published updates…"));
        QSettings().setValue("updates/lastCheck",QDateTime::currentDateTimeUtc());
        QNetworkRequest request(QUrl("https://api.github.com/repos/rhamenator/"+repo_+"/releases?per_page=20"));
        request.setRawHeader("User-Agent","SoundCurrent-update-check");request.setRawHeader("Accept","application/vnd.github+json");request.setTransferTimeout(10000);
        auto *reply=network_.get(request);auto bytes=std::make_shared<QByteArray>();
        connect(reply,&QNetworkReply::readyRead,this,[reply,bytes]{if(reply->bytesAvailable()+bytes->size()>1024*1024){reply->abort();return;}bytes->append(reply->readAll());});
        connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,manual]{
            busy_=false;bytes->append(reply->readAll());
            if(reply->error()!=QNetworkReply::NoError || bytes->size()>1024*1024){
                if(manual)status_->setText(SC_TR("Published releases could not be checked. Private Studio releases require GitHub access. Use Open release downloads; downloaded installers are still detected locally."));
                reply->deleteLater();return;}
            QJsonParseError error;const auto document=QJsonDocument::fromJson(*bytes,&error);
            if(error.error!=QJsonParseError::NoError || !document.isArray()){if(manual)status_->setText(SC_TR("The update response was invalid. No installer was opened."));reply->deleteLater();return;}
            const auto newest=publishedUpdate(document.array(),repo_,version_,preview_->isChecked());
            if(newerUpdate(newest,version_))remind(SC_TR("Published update %1 is available. Open release downloads, then install over this version and reopen.").arg(newest));
            else if(manual){status_->setText(SC_TR("No newer published release found. Downloaded installers are also checked."));checkLocal();}
            reply->deleteLater();});
    }
    QString repo_,title_,version_,folder_,lastReminder_;QNetworkAccessManager network_;QTimer timer_;QLabel *status_;QCheckBox *enabled_,*preview_;qint64 size_=0;QDateTime modified_;bool busy_=false;
};
}
