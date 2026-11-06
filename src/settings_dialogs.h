// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization_text.h"
#include <QDialog>
#include <algorithm>
#include <QGroupBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QSlider>
#include <QScrollBar>
#include <QWheelEvent>
#include <QCoreApplication>
namespace soundcurrent::ui {
class ScrollControlGuard final : public QObject {
public:
    explicit ScrollControlGuard(QScrollArea *page):QObject(page),page_(page){}
    bool eventFilter(QObject *,QEvent *event) override {
        if(event->type()!=QEvent::Wheel)return false;
        auto *wheel=static_cast<QWheelEvent *>(event);auto *bar=page_->verticalScrollBar();
        QWheelEvent forwarded(QPointF(bar->rect().center()),wheel->globalPosition(),wheel->pixelDelta(),wheel->angleDelta(),wheel->buttons(),wheel->modifiers(),wheel->phase(),wheel->inverted(),wheel->source());
        QCoreApplication::sendEvent(bar,&forwarded);return true;
    }
private:QScrollArea *page_;
};
inline void protectScrollControls(QScrollArea *page){
    auto *guard=new ScrollControlGuard(page);
    for(auto *widget:page->findChildren<QWidget *>())
        if(qobject_cast<QAbstractSpinBox *>(widget) || qobject_cast<QComboBox *>(widget) || qobject_cast<QSlider *>(widget))widget->installEventFilter(guard);
}

inline QDialog *settingsSection(QVBoxLayout *settings,QGroupBox *section,QWidget *owner,const QString &id){
    settings->removeWidget(section);auto *dialog=new QDialog(owner);dialog->setObjectName(id);dialog->setProperty("soundcurrentSetupDialog",true);dialog->setWindowTitle(section->title().replace("&&","&"));dialog->setWindowModality(Qt::ApplicationModal);
    auto *layout=new QVBoxLayout(dialog);auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setObjectName(id+"Scroll");scroll->setWidget(section);layout->addWidget(scroll,1);
    auto *close=new QPushButton(SC_TR("Close"));layout->addWidget(close);QObject::connect(close,&QPushButton::clicked,dialog,&QDialog::reject);
    auto *launch=new QPushButton(section->title());launch->setObjectName(id+"Open");launch->setAccessibleDescription(section->title());settings->addWidget(launch);
    QObject::connect(launch,&QPushButton::clicked,dialog,[dialog]{const auto area=dialog->screen()->availableGeometry();dialog->resize(std::min(800,area.width()-30),std::min(640,area.height()-40));dialog->exec();});
    return dialog;
}
}
