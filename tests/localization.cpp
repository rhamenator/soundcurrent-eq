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
  require(resolve("nn-NO")=="nn" && resolve("nb-NO")=="nb","Norwegian written standards collapsed");
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
   require(!translator.translate("SoundCurrent","Equalizer").isEmpty(),"Language catalog is empty");
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
   QSettings().setValue("i18n/language","nn");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("Kvalitetsfaktor Q for filteret"),"Nynorsk filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("Lagre"),"Nynorsk standard actions stayed English");
   for(const auto &language:languages())if(language.tag=="nn")require(language.translated==language.total,"Nynorsk catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","ar");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("معامل جودة المرشح Q"),"Arabic filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("حفظ"),"Arabic standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::RightToLeft,"Arabic layout is not RTL");
   for(const auto &language:languages())if(language.tag=="ar")require(language.translated==language.total,"Arabic catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","he");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("מקדם האיכות של המסנן Q"),"Hebrew filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("שמירה"),"Hebrew standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::RightToLeft,"Hebrew layout is not RTL");
   for(const auto &language:languages())if(language.tag=="he")require(language.translated==language.total,"Hebrew catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","fa");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("ضریب کیفیت فیلتر Q"),"Persian filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("ذخیره"),"Persian standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::RightToLeft,"Persian layout is not RTL");
   for(const auto &language:languages())if(language.tag=="fa")require(language.translated==language.total,"Persian catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","zh-Hans");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("滤波器品质因数 Q"),"Simplified Chinese filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("保存"),"Simplified Chinese standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::LeftToRight,"Simplified Chinese layout is not LTR");
   require(resolve("zh-CN")=="zh-Hans" && resolve("zh-TW")=="zh-Hant","Chinese script selection collapsed");
   for(const auto &language:languages())if(language.tag=="zh-Hans")require(language.translated==language.total,"Simplified Chinese catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","zh-Hant");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("濾波器品質因數 Q"),"Traditional Chinese filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("儲存"),"Traditional Chinese standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::LeftToRight,"Traditional Chinese layout is not LTR");
   require(resolve("zh-CN")=="zh-Hans" && resolve("zh-TW")=="zh-Hant","Chinese script selection collapsed");
   for(const auto &language:languages())if(language.tag=="zh-Hant")require(language.translated==language.total,"Traditional Chinese catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","ja");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("フィルターの Q 値"),"Japanese filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("保存"),"Japanese standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::LeftToRight,"Japanese layout is not LTR");
   require(resolve("ja-JP")=="ja","Japanese regional fallback incorrect");
   for(const auto &language:languages())if(language.tag=="ja")require(language.translated==language.total,"Japanese catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","ko");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("필터 Q"),"Korean filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("저장"),"Korean standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::LeftToRight,"Korean layout is not LTR");
   require(resolve("ko-KR")=="ko","Korean regional fallback incorrect");
   for(const auto &language:languages())if(language.tag=="ko")require(language.translated==language.total,"Korean catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","hi");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("फ़िल्टर Q"),"Hindi filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("सहेजें"),"Hindi standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::LeftToRight,"Hindi layout is not LTR");
   require(resolve("hi-IN")=="hi","Hindi regional fallback incorrect");
   for(const auto &language:languages())if(language.tag=="hi")require(language.translated==language.total,"Hindi catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","id");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("Q filter"),"Indonesian filter quality caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Close|QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("Simpan"),"Indonesian standard actions stayed English");
   require(QApplication::layoutDirection()==Qt::LeftToRight,"Indonesian layout is not LTR");
   require(resolve("id-ID")=="id","Indonesian regional fallback incorrect");
   for(const auto &language:languages())if(language.tag=="id")require(language.translated==language.total,"Indonesian catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","vi");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("Q của bộ lọc"),"Vietnamese filter Q caption incorrect");
   QDialogButtonBox buttons(QDialogButtonBox::Save);
   require(buttons.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("Lưu"),"Vietnamese standard Save button incorrect");
   require(qApp->layoutDirection()==Qt::LeftToRight,"Vietnamese direction incorrect");
   require(resolve("vi-VN")=="vi","Vietnamese regional fallback incorrect");
   for(const auto &language:languages())if(language.tag=="vi")require(language.translated==language.total,"Vietnamese catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","th");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("ค่า Q ของฟิลเตอร์"),"Thai filter Q caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("บันทึก"),"Thai standard Save button incorrect");
   require(QApplication::layoutDirection()==Qt::LeftToRight,"Thai direction incorrect");
   require(resolve("th-TH")=="th","Thai regional fallback incorrect");
   for(const auto &language:languages())if(language.tag=="th")require(language.translated==language.total,"Thai catalog has missing messages");
  }
  {
   QSettings().setValue("i18n/language","sw");Runtime runtime;runtime.initialize();
   require(text("Filter Q")==QString::fromUtf8("Q ya kichujio"),"Swahili filter Q caption incorrect");
   QDialogButtonBox actions(QDialogButtonBox::Save);
   require(actions.button(QDialogButtonBox::Save)->text()==QString::fromUtf8("Hifadhi"),"Swahili standard Save button incorrect");
   require(QApplication::layoutDirection()==Qt::LeftToRight,"Swahili direction incorrect");
   require(resolve("sw-TZ")=="sw","Swahili regional fallback incorrect");
   for(const auto &language:languages())if(language.tag=="sw")require(language.translated==language.total,"Swahili catalog has missing messages");
  }
  for(const auto &language:languages()) {
   if(language.tag=="en")continue;
   QSettings().setValue("i18n/language",language.tag);Runtime runtime;runtime.initialize();
   for(const char *source:{"Selected speakers are disconnected","Audio bridge did not start","Install the Windows audio route using Audio driver setup, then reopen the app."}) {
    const auto translated=text(source);
    require(!translated.isEmpty()&&translated!=QString::fromUtf8(source),"Windows adapter message fell back to English in a populated locale");
    try {throw std::runtime_error(translated.toStdString());}
    catch(const std::runtime_error &error) {require(QString::fromUtf8(error.what())==translated,"Translated adapter exception lost Unicode text");}
   }
  }
  {
   QSettings().setValue("i18n/language","qps-rtl");Runtime runtime;runtime.initialize();
   auto pattern=text("Value %1 / %2");require(pattern.contains("%1")&&pattern.contains("%2"),"Pseudo locale damaged placeholders");
   require(QApplication::layoutDirection()==Qt::RightToLeft,"RTL layout missing");
  }
  qInfo("PASS: embedded global catalogs, regional/script fallback, decimal input, invariant JSON and RTL placeholders");return 0;
 }catch(const std::exception&e){qCritical("%s",e.what());return 1;}
}
