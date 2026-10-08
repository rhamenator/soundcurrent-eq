// SPDX-License-Identifier: GPL-3.0-only
#include "localization.h"
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QJsonDocument>
#include <QLineEdit>
#include <QTemporaryDir>
#include <stdexcept>
using namespace soundcurrent::i18n;
void require(bool pass,const char *why){if(!pass)throw std::runtime_error(why);}
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setOrganizationName("SoundCurrent");app.setApplicationName("localization-test");
 QTemporaryDir dir;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());
 try{
  require(languages().size()>=30,"Global language catalogs missing");
  require(resolve("fr_CA")=="fr","Regional fallback failed");
  require(resolve("zh-TW")=="zh-Hant" && resolve("zh-CN")=="zh-Hans","Chinese region aliases fell back to English");
  require(resolve("pt-Latn-BR")=="pt-BR" && resolve("pt-Latn-PT")=="pt-PT","Explicit Latin script lost Portuguese regions");
  require(resolve("de-DE-u-nu-latn")=="de","Unicode locale extension blocked language fallback");
  require(resolve("pt-Cyrl-BR")=="en" && resolve("zh-Kore-TW")=="en","Unsupported explicit script was guessed");
  require(resolve("fr--CA")=="en","Malformed language tag was accepted");
  require(resolve("pt-BR")=="pt-BR" && resolve("pt-PT")=="pt-PT","Distinct Portuguese regions collapsed");
  require(resolve("zh-Hant-TW")=="zh-Hant" && resolve("zh-Hans-CN")=="zh-Hans","Chinese scripts collapsed");
  require(resolve("sr-Latn")=="en","An absent explicit script was guessed");
  require(resolve("../bad")=="en","Unsupported language should fall back safely");
  for(const auto &l:languages()) {
   QTranslator translator;require(translator.load(":/i18n/soundcurrent_"+l.tag+".qm"),"Embedded catalog failed to load");
   require(!translator.translate("SoundCurrent","Equalizer").isEmpty(),"Draft language is empty");
  }
  QSettings().setValue("i18n/language","de");QSettings().setValue("i18n/formatLocale","de-DE");
  {
   Runtime runtime;runtime.initialize();require(text("Output device")==QString::fromUtf8("Ausgabegerät"),"German catalog not active");
   require(text("Untranslated sample")=="Untranslated sample","English fallback failed");
   require(text("Filter Q")==QString::fromUtf8("Filtergüte Q"),"Filter quality factor was mislabeled as bandwidth");
   QDialogButtonBox germanActions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(germanActions.button(QDialogButtonBox::Close)->text()==QString::fromUtf8("Schließen"),"German standard actions stayed English");
   for(const auto &language:languages())if(language.tag=="de")require(language.translated==language.total,"German catalog has missing messages");
   std::unique_ptr<QGroupBox> panel(settingsPanel());
   auto *format=panel->findChild<QComboBox *>("formatLocale");
   require(format->findData("de-DE")>=0 && format->findData("fr-CA")>=0,"Explicit formatting regions missing");
   QDoubleSpinBox spin;spin.setRange(-60,12);spin.setDecimals(1);spin.setValue(1.5);
   require(spin.text().contains(','),"Decimal comma missing");
   spin.findChild<QLineEdit *>()->setText("-12,5");spin.interpretText();require(spin.value()==-12.5,"Localized numeric input changed value");
   auto json=QJsonDocument(QJsonObject{{"gain",spin.value()},{"presetId","Flat"}}).toJson(QJsonDocument::Compact);
   require(json.contains("-12.5") && !json.contains("-12,5"),"Machine state became locale-dependent");
  }
  {
   QSettings().setValue("i18n/language","fr");Runtime runtime;runtime.initialize();
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Cancel|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Close)->text()==QString::fromUtf8("Fermer"),"Qt Close action stayed English");
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("Enregistrer"),"Qt Save action stayed English");
   require(text("Import JSON")==QString::fromUtf8("Importer un fichier JSON"),"Equipment actions stayed English");
   const auto prompt=text("%1 / %2\n%3\nApply this correction to the %4 route?").arg("FixtureBrand","FixtureModel","FixtureConditions",text("Speaker"));
   require(prompt.contains("FixtureBrand")&&prompt.contains("FixtureConditions")&&!prompt.contains("%4"),"Translated confirmation lost equipment details");
   for(const auto &language:languages())if(language.tag=="fr")require(language.translated==language.total,"French catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","es");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("Factor Q del filtro"),"Spanish filter quality caption incorrect");
   require(text("Flat")==QString::fromUtf8("Plano"),"Spanish flat preset caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Close)->text()==QString::fromUtf8("Cerrar"),"Spanish standard actions stayed English");
   for(const auto &language:languages())if(language.tag=="es")require(language.translated==language.total,"Spanish catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","it");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("Fattore Q del filtro"),"Italian filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Close)->text()==QString::fromUtf8("Chiudi"),"Italian standard actions stayed English");
   for(const auto &language:languages())if(language.tag=="it")require(language.translated==language.total,"Italian catalog has missing messages");
  }
  for(const auto &tag: {QString("pt-PT"),QString("pt-BR")}) {
   QSettings().setValue("i18n/language",tag);Runtime runtime;runtime.initialize();
   require(text("Speaker")==QString::fromUtf8(tag=="pt-PT"?"Coluna":"Alto-falante"),"Portuguese regional equipment term incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8(tag=="pt-PT"?"Guardar":"Salvar"),"Portuguese regional standard action incorrect");
   for(const auto &language:languages())if(language.tag==tag)require(language.translated==language.total,"Portuguese catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","nl");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("Filter-Q"),"Dutch filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("Opslaan"),"Dutch standard actions stayed English");
   for(const auto &language:languages())if(language.tag=="nl")require(language.translated==language.total,"Dutch catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","pl");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("Dobroć filtra Q"),"Polish filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("Zapisz"),"Polish standard actions stayed English");
   for(const auto &language:languages())if(language.tag=="pl")require(language.translated==language.total,"Polish catalog has missing messages");
  }
  for (const auto &tag : {QStringLiteral("cs"), QStringLiteral("sk")}) {
   QSettings().setValue("i18n/language",tag);Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8(tag=="cs"?"Činitel jakosti filtru Q":"Činiteľ akosti filtra Q"),"Czech/Slovak filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8(tag=="cs"?"Uložit":"Uložiť"),"Czech/Slovak standard actions stayed English");
   for(const auto &language:languages())if(language.tag==tag)require(language.translated==language.total,"Czech/Slovak catalog has missing messages");
  }
  for (const auto &tag : {QStringLiteral("uk"), QStringLiteral("ru")}) {
   QSettings().setValue("i18n/language",tag);Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8(tag=="uk"?"Добротність фільтра Q":"Добротность фильтра Q"),"Ukrainian/Russian filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8(tag=="uk"?"Зберегти":"Сохранить"),"Ukrainian/Russian standard actions stayed English");
   for(const auto &language:languages())if(language.tag==tag)require(language.translated==language.total,"Ukrainian/Russian catalog has missing messages");
  }
  for (const auto &tag : {QStringLiteral("el"), QStringLiteral("tr")}) {
   QSettings().setValue("i18n/language",tag);Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8(tag=="el"?"Συντελεστής ποιότητας φίλτρου Q":"Filtre kalite faktörü Q"),"Greek/Turkish filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8(tag=="el"?"Αποθήκευση":"Kaydet"),"Greek/Turkish standard actions stayed English");
   for(const auto &language:languages())if(language.tag==tag)require(language.translated==language.total,"Greek/Turkish catalog has missing messages");
  }
  for (const auto &tag : {QStringLiteral("sv"), QStringLiteral("da")}) {
   QSettings().setValue("i18n/language",tag);Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8(tag=="sv"?"Filtrets kvalitetsfaktor Q":"Filterets kvalitetsfaktor Q"),"Swedish/Danish filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8(tag=="sv"?"Spara":"Gem"),"Swedish/Danish standard actions stayed English");
   for(const auto &language:languages())if(language.tag==tag)require(language.translated==language.total,"Swedish/Danish catalog has missing messages");
  }
  for (const auto &tag : {QStringLiteral("nb"), QStringLiteral("fi")}) {
   QSettings().setValue("i18n/language",tag);Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8(tag=="nb"?"Filterets kvalitetsfaktor Q":"Suodattimen laatutekijä Q"),"Norwegian/Finnish filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8(tag=="nb"?"Lagre":"Tallenna"),"Norwegian/Finnish standard actions stayed English");
   for(const auto &language:languages())if(language.tag==tag)require(language.translated==language.total,"Norwegian/Finnish catalog has missing messages");
  }
  for (const auto &tag : {QStringLiteral("ro"), QStringLiteral("hu")}) {
   QSettings().setValue("i18n/language",tag);Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8(tag=="ro"?"Factor de calitate Q al filtrului":"Szűrő jósági tényezője Q"),"Romanian/Hungarian filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8(tag=="ro"?"Salvați":"Mentés"),"Romanian/Hungarian standard actions stayed English");
   for(const auto &language:languages())if(language.tag==tag)require(language.translated==language.total,"Romanian/Hungarian catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","qps-rtl");Runtime runtime;runtime.initialize();
   auto pattern=text("Value %1 / %2");require(pattern.contains("%1")&&pattern.contains("%2"),"Pseudo locale damaged placeholders");
   require(QApplication::layoutDirection()==Qt::RightToLeft,"RTL layout missing");
  }
  qInfo("PASS: embedded global catalogs, regional/script fallback, decimal input, invariant JSON and RTL placeholders");return 0;
 }catch(const std::exception&e){qCritical("%s",e.what());return 1;}
}
