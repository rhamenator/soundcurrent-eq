// SPDX-License-Identifier: GPL-3.0-only
#include "update_panel.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char **argv){QApplication app(argc,argv);try{
 using namespace soundcurrent;
 require(newerUpdate("parity-preview-0.7.5","0.7.4")&&newerUpdate("v1.0.0","0.9.9"),"version ordering");
 require(!newerUpdate("0.7.3","0.7.4")&&!newerUpdate("invalid","0.7.4")&&!newerUpdate("999999999.1.0","0.7.4"),"invalid/downgrade version");
 QTemporaryDir folder;require(folder.isValid(),"temp folder");
#ifdef Q_OS_WIN
 const QString prefix="SoundCurrent-EQ-",suffix="-windows-x64-setup.exe";
#else
 const QString prefix="soundcurrent-eq_",suffix="_amd64.deb";
#endif
 for(const QString version:{QString("0.7.3"),QString("0.7.6"),QString("0.7.5")}){QFile f(folder.filePath(prefix+version+suffix));require(f.open(QIODevice::WriteOnly),"test file");f.write("fixture");}
 require(downloadedUpdate("soundcurrent-eq","0.7.4",folder.path()).first=="0.7.6","newest local update selection");
 require(downloadedUpdate("soundcurrent-studio","0.8.3",folder.path()).second.isEmpty(),"other app installer suggested");
 auto release=[](const QString &tag,const QString &url,bool preview=false,bool draft=false){return QJsonObject{{"tag_name",tag},{"html_url",url},{"prerelease",preview},{"draft",draft}};};
 QJsonArray releases{release("0.7.5","https://github.com/rhamenator/soundcurrent-eq/releases/tag/0.7.5"),release("0.7.8","https://example.org/",false),release("0.7.9","https://github.com/rhamenator/soundcurrent-studio/releases/tag/0.7.9"),release("preview-0.7.7","https://github.com/rhamenator/soundcurrent-eq/releases/tag/preview-0.7.7",true),release("0.7.10","https://github.com/rhamenator/soundcurrent-eq/releases/tag/0.7.10",false,true)};
 require(publishedUpdate(releases,"soundcurrent-eq","0.7.4",false)=="0.7.5","stable/draft/source filtering");
 require(publishedUpdate(releases,"soundcurrent-eq","0.7.4",true)=="0.7.7","preview selection");
 std::cout<<"PASS: update version ordering, local package selection, app isolation, published preview/draft/source policy; no network or installation performed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
