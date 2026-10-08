// SPDX-License-Identifier: GPL-3.0-only
#include "audio_error_text.h"
#include <QCoreApplication>
#include <QTranslator>
#include <stdexcept>
void require(bool pass) {if(!pass)throw std::runtime_error("Backend diagnostic translation invariant failed");}
class Fixture : public QTranslator {
 public:
 bool isEmpty() const override {return false;}
 QString translate(const char *context,const char *source,const char *,int) const override {
  if(QByteArray(context)!="SoundCurrent")return {};
  if(QByteArray(source)=="Read speaker level")return "LEVEL";
  if(QByteArray(source)=="%1 failed (0x%2)")return "%1 | %2";
  return {};
 }
};
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv);Fixture fixture;app.installTranslator(&fixture);
 using soundcurrent::i18n::audioErrorText;
 require(audioErrorText("Read speaker level failed (0xAbCd1234)")=="LEVEL | AbCd1234");
 require(audioErrorText("External driver failed (0x80070005)")=="External driver failed (0x80070005)");
 require(audioErrorText("Read speaker level failed (0xZZ)")=="Read speaker level failed (0xZZ)");
}
