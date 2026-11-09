// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QApplication>
#include <QAbstractButton>
#include <QBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QMessageBox>
#include <QScreen>
#include <QScrollArea>
#include <QTimer>
#include <QWidget>
#include <algorithm>
namespace soundcurrent::ui {
// A short control row becomes a stack when translated labels / font scaling
// need more width. It reports a height-for-width to the containing layout.
class ResponsiveRow final : public QWidget {
 QBoxLayout *row_=new QBoxLayout(QBoxLayout::LeftToRight,this);
 void requireCalculatedHeight(){const int required=heightForWidth(std::max(1,width()));if(minimumHeight()!=required)setMinimumHeight(required);}
public:
 explicit ResponsiveRow(QWidget *parent=nullptr):QWidget(parent){
  row_->setContentsMargins(0,0,0,0);row_->setSizeConstraint(QLayout::SetNoConstraint);
  QSizePolicy policy(QSizePolicy::Expanding,QSizePolicy::Minimum);policy.setHeightForWidth(true);setSizePolicy(policy);
 }
 void addWidget(QWidget *widget,int stretch=0){row_->addWidget(widget,stretch);requireCalculatedHeight();updateGeometry();}
 void addStretch(){row_->addStretch();requireCalculatedHeight();updateGeometry();}
 int naturalWidth()const{int result=0,count=0;for(int i=0;i<row_->count();++i)if(auto *w=row_->itemAt(i)->widget();w && !w->isHidden()){result+=w->sizeHint().width();++count;}return result+std::max(0,count-1)*row_->spacing();}
 bool hasHeightForWidth()const override{return true;}
 int heightForWidth(int width)const override{int height=0,count=0;bool stack=naturalWidth()>width;for(int i=0;i<row_->count();++i)if(auto*w=row_->itemAt(i)->widget();w && !w->isHidden()){height=stack?height+w->sizeHint().height():std::max(height,w->sizeHint().height());++count;}return height+(stack?std::max(0,count-1)*row_->spacing():0);}
 QSize sizeHint()const override{return {naturalWidth(),heightForWidth(std::max(1,naturalWidth()))};}
 QSize minimumSizeHint()const override{
  // Layouts using SetNoConstraint may report an invalid minimum for compound
  // fields.  Their normal height remains mandatory: allowing zero here can
  // collapse combo boxes and complete editor rows inside a scroll page.
  int height=0;for(int i=0;i<row_->count();++i)if(auto*w=row_->itemAt(i)->widget();w && !w->isHidden())height=std::max(height,w->sizeHint().height());
  return {0,height};
 }
protected:
 void changeEvent(QEvent *event)override{QWidget::changeEvent(event);if(event->type()==QEvent::FontChange || event->type()==QEvent::LayoutDirectionChange){row_->invalidate();row_->setDirection(naturalWidth()>width()?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);requireCalculatedHeight();updateGeometry();}}
 void resizeEvent(QResizeEvent *event)override{QWidget::resizeEvent(event);const auto direction=naturalWidth()>width()?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight;if(row_->direction()!=direction){row_->setDirection(direction);updateGeometry();}requireCalculatedHeight();}
};
// Only owned setup dialogs are adapted; native file pickers and message boxes
// retain their platform behavior. Existing scroll bodies keep all controls
// reachable, and their footer actions remain outside the scrollable body.
class ResponsiveDialogs final : public QObject {
 bool pending_=false;
 void fit(QDialog *dialog){
  if(!dialog || !dialog->isVisible())return;
  auto *display=dialog->screen();if(!display)return;
  const auto available=display->availableGeometry().adjusted(12,12,-12,-12);
  const auto frame=dialog->frameGeometry().size()-dialog->size();
  const QSize maximum(std::max(1,available.width()-std::max(0,frame.width())),std::max(1,available.height()-std::max(0,frame.height())));
  dialog->setMinimumSize(0,0);
  if(dialog->layout())dialog->layout()->setSizeConstraint(QLayout::SetNoConstraint);
  for(auto *scroll:dialog->findChildren<QScrollArea*>()){scroll->setMinimumSize(0,0);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);}
  for(auto *form:dialog->findChildren<QFormLayout*>())form->setRowWrapPolicy(QFormLayout::WrapLongRows);
  for(auto *buttons:dialog->findChildren<QDialogButtonBox*>()){
   int width=0;for(auto *button:buttons->buttons())width+=button->sizeHint().width()+8;
   buttons->setOrientation(width>std::min(maximum.width(),dialog->width())-30?Qt::Vertical:Qt::Horizontal);
  }
  dialog->resize(dialog->size().boundedTo(maximum));
  auto origin=dialog->frameGeometry().topLeft();
  origin.setX(std::clamp(origin.x(),available.left(),std::max(available.left(),available.right()-dialog->frameGeometry().width()+1)));
  origin.setY(std::clamp(origin.y(),available.top(),std::max(available.top(),available.bottom()-dialog->frameGeometry().height()+1)));
  dialog->move(origin);
 }
public:
 explicit ResponsiveDialogs(QObject *parent):QObject(parent){}
 bool eventFilter(QObject *object,QEvent *event)override{
  auto *dialog=qobject_cast<QDialog*>(object);
  if(!dialog || qobject_cast<QFileDialog*>(dialog) || qobject_cast<QMessageBox*>(dialog))return false;
  const auto type=event->type();
  if(type==QEvent::Show || type==QEvent::ScreenChangeInternal || type==QEvent::FontChange || type==QEvent::ApplicationFontChange || type==QEvent::Resize){
   if(!pending_){pending_=true;QTimer::singleShot(0,this,[this]{pending_=false;for(auto*w:QApplication::topLevelWidgets())if(auto*d=qobject_cast<QDialog*>(w);d && !qobject_cast<QFileDialog*>(d) && !qobject_cast<QMessageBox*>(d))fit(d);});}
  }
  return false;
 }
};
inline void installResponsiveDialogs(){static auto *filter=new ResponsiveDialogs(qApp);static bool installed=false;if(!installed){qApp->installEventFilter(filter);installed=true;}}
}
