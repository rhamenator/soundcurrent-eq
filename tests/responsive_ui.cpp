// SPDX-License-Identifier: GPL-3.0-only
#include "responsive_ui.h"
#include <QPushButton>
#include <QLineEdit>
#include <QScrollBar>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
#include <stdexcept>
void check(bool okay,const char *message){if(!okay)throw std::runtime_error(message);}
void settle(){QElapsedTimer timer;timer.start();while(timer.elapsed()<80){QApplication::processEvents();QThread::msleep(1);}}
int main(int argc,char **argv){QApplication app(argc,argv);try{
 soundcurrent::ui::installResponsiveDialogs();
 QWidget host;auto *root=new QVBoxLayout(&host);auto *row=new soundcurrent::ui::ResponsiveRow;
 auto *left=new QPushButton("Long translated output controls"),*right=new QPushButton("More translated channel controls");
 row->addWidget(left);row->addWidget(right);root->addWidget(row);host.resize(260,180);host.show();settle();
 check(left->geometry().bottom()<right->geometry().top(),"Narrow controls failed to stack");
 host.resize(1200,180);settle();check(left->geometry().top()==right->geometry().top(),"Wide controls failed to return to a row");
 auto font=app.font();font.setPointSizeF(font.pointSizeF()*1.75);app.setFont(font);host.resize(400,220);settle();
 check(left->geometry().bottom()<right->geometry().top(),"Scaled controls failed to stack");
 QDialog dialog;auto *outer=new QVBoxLayout(&dialog);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);
 auto *body=new QWidget;auto *form=new QFormLayout(body);QLineEdit *last=nullptr;
 for(int i=0;i<40;++i){last=new QLineEdit;form->addRow("Translated configuration field "+QString::number(i),last);}
 scroll->setWidget(body);outer->addWidget(scroll,1);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);outer->addWidget(buttons);
 dialog.resize(2000,2000);dialog.show();settle();
 check(dialog.screen()->availableGeometry().contains(dialog.frameGeometry()),"Scaled dialog escaped available screen");
 check(dialog.rect().contains(QRect(buttons->mapTo(&dialog,QPoint()),buttons->size())),"Dialog footer became inaccessible");
 scroll->ensureWidgetVisible(last);settle();
 check(scroll->viewport()->rect().intersects(QRect(last->mapTo(scroll->viewport(),QPoint()),last->size())),"Last setup field cannot be reached by scrolling");
 check(form->rowWrapPolicy()==QFormLayout::WrapLongRows,"Translated form labels cannot wrap");
 std::cout<<"PASS: responsive rows, font scaling, dialog fit, pinned actions and scroll reachability\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
