#include "localization.h"
#include <QFileDialog>
#include <QLabel>
#include <QAbstractButton>
#include <QTextStream>
int main(int argc,char **argv){QApplication app(argc,argv);QTranslator catalog;catalog.load("/home/rich/dev/soundcurrent-eq/data/localization/soundcurrent_fr.qm");soundcurrent::i18n::StandardActionTranslator standard;app.installTranslator(&standard);app.installTranslator(&catalog);QFileDialog dialog;dialog.setOption(QFileDialog::DontUseNativeDialog);dialog.setDirectory("/tmp");dialog.setNameFilter("Equipment profile (*.json)");dialog.show();app.processEvents();QTextStream out(stdout);for(auto *w:dialog.findChildren<QLabel*>())out<<"label: "<<w->text()<<"\n";for(auto *w:dialog.findChildren<QAbstractButton*>())out<<"button: "<<w->text()<<"; tooltip: "<<w->toolTip()<<"\n";}
