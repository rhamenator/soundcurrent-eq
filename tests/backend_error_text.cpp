// SPDX-License-Identifier: GPL-3.0-only
#include "audio_error_text.h"
#include "audio_setup_arguments.h"
#include <QCoreApplication>
#include <QProcess>
#include <QTranslator>
#include <QTextStream>
#include <stdexcept>
void require(bool pass) {if(!pass)throw std::runtime_error("Backend diagnostic translation invariant failed");}
class Fixture : public QTranslator {
 public:
 bool isEmpty() const override {return false;}
 QString translate(const char *context,const char *source,const char *,int) const override {
  if(QByteArray(context)!="SoundCurrent")return {};
  if(QByteArray(source).startsWith("Invalid ") || QByteArray(source).startsWith("Too many ") || QByteArray(source)=="Duplicate Studio route" || QByteArray(source)=="Enhancements outside supported ranges" || QByteArray(source).startsWith("Shared and channel EQ"))return "OWNED_DIAGNOSTIC";
  if(QByteArray(source)=="Read speaker level")return "LEVEL";
  if(QByteArray(source)=="Unsupported Studio profile schema")return "PROFILE_SCHEMA";
  if(QByteArray(source)=="Studio profile has an invalid numeric field")return "PROFILE_NUMBER";
  if(QByteArray(source)=="Studio profile has an invalid boolean field")return "PROFILE_BOOLEAN";
  if(QByteArray(source)=="%1 failed (0x%2)")return "%1 | %2";
  return {};
 }
};
int main(int argc,char **argv) try {
 QCoreApplication app(argc,argv);Fixture fixture;app.installTranslator(&fixture);
 using soundcurrent::i18n::audioErrorText;
 const auto unicodeOutput=QString::fromUtf8("Réparation / 日本語 / العربية / %1 / 音声");
 const auto outputBytes=unicodeOutput.toUtf8()+"\r\n";
 QByteArray accumulated;
 for(const char byte:outputBytes)accumulated.append(byte);
 require(soundcurrent::i18n::audioSetupOutput(accumulated)==unicodeOutput);
 const auto fixtureIndex=app.arguments().indexOf("--powershell-output-fixture");
 if(fixtureIndex>=0) {
  require(fixtureIndex+2<app.arguments().size());
  QProcess child;child.setProgram(app.arguments()[fixtureIndex+1]);
  child.setArguments({"-NoProfile","-NonInteractive","-ExecutionPolicy","RemoteSigned","-File",app.arguments()[fixtureIndex+2]});
  child.start();
  const bool finished=child.waitForFinished(15000);
  const auto standardOutput=child.readAllStandardOutput();
  const auto standardError=child.readAllStandardError();
  const bool successful=finished && child.exitStatus()==QProcess::NormalExit && child.exitCode()==0;
  const bool correctOutput=soundcurrent::i18n::audioSetupOutput(standardOutput)==unicodeOutput;
  if(!successful || !correctOutput) {
   // Inert fixture only. Bound diagnostics; bytes expose encoding differences
   // without relying on the runner's console code page to render them.
   QTextStream diagnostic(stderr);
   diagnostic << "PowerShell output fixture failed: finished=" << finished
              << " processError=" << static_cast<int>(child.error())
              << " exitStatus=" << static_cast<int>(child.exitStatus())
              << " exitCode=" << child.exitCode() << '\n'
              << "processErrorText=" << child.errorString() << '\n'
              << "stdoutHex=" << standardOutput.left(4096).toHex() << '\n'
              << "stderrHex=" << standardError.left(4096).toHex() << '\n'
              << "expectedHex=" << unicodeOutput.toUtf8().toHex() << '\n';
   diagnostic.flush();
   if(!finished) {child.kill();child.waitForFinished(5000);}
   return 1;
  }
 }
 const auto setupScript=QString::fromUtf8("C:/Program Files/SoundCurrent/音声 setup.ps1");
 for(const auto &language:QStringList{"fr","nn","zh-Hant","en","qps-rtl"}) {
  for(bool install:{false,true}) {
   const auto args=soundcurrent::i18n::audioSetupArguments(setupScript,install,1234,language);
   require(args==QStringList{"-NoProfile","-ExecutionPolicy","RemoteSigned","-File",setupScript,
                            install?"-Install":"-Settings","-Quiet","-RequestingProcessId","1234","-Language",language});
  }
 }
 require(audioErrorText("Unsupported Studio profile schema")=="PROFILE_SCHEMA");
 require(audioErrorText("Studio profile has an invalid numeric field")=="PROFILE_NUMBER");
 require(audioErrorText("Studio profile has an invalid boolean field")=="PROFILE_BOOLEAN");
 require(audioErrorText("Invalid Studio channel count") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid Studio profile channel count") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid Studio channel name or filters") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Too many Studio channel filters") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid Studio route") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Duplicate Studio route") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid route indexes or weight") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid route number") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Too many Studio routes") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid filter type") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Shared and channel EQ exceed 64 filters; remove some channel filters") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid enhancement parameter count") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid enhancement parameter type") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Enhancements outside supported ranges") == "OWNED_DIAGNOSTIC");
 require(audioErrorText("Invalid external route %1")=="Invalid external route %1");
 require(audioErrorText("External profile %1 — unknown")=="External profile %1 — unknown");
 require(audioErrorText("Read speaker level failed (0xAbCd1234)")=="LEVEL | AbCd1234");
 require(audioErrorText("External driver failed (0x80070005)")=="External driver failed (0x80070005)");
 require(audioErrorText("Read speaker level failed (0xZZ)")=="Read speaker level failed (0xZZ)");
} catch(const std::exception &error) {
 QTextStream diagnostic(stderr);
 diagnostic << error.what() << '\n';
 diagnostic.flush();
 return 1;
}
