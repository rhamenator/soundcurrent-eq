#include "localization.h"
// SPDX-License-Identifier: GPL-3.0-only
#include "accelerating_spinbox.h"
#pragma once
#include "enhancement.h"
#include <QGroupBox>
#include <QFormLayout>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QSignalBlocker>
#include <functional>
#include <cmath>
namespace soundcurrent {
class EnhancementControls : public QGroupBox {
public:
    explicit EnhancementControls(bool advanced,QWidget *parent=nullptr):QGroupBox(SC_TR("Sound enhancements"),parent) {
        auto *form=new QFormLayout(this);
        auto *help=new QLabel(advanced?SC_TR("Front L/R enhancements (mono supported); other channels keep their own Studio effects. Zero amounts bypass each enhancement."):SC_TR("Zero turns each effect off. These listening effects apply to speaker playback, not microphone correction."));help->setWordWrap(true);form->addRow(help);
        for(std::size_t i=0;i<5;++i){auto *row=new QHBoxLayout;sliders_[i]=new QSlider(Qt::Horizontal);sliders_[i]->setRange(0,100);sliders_[i]->setAccessibleName(soundcurrent::i18n::text(effectParameters[i].name));
            labels_[i]=new QLabel("0%");labels_[i]->setMinimumWidth(42);row->addWidget(sliders_[i]);row->addWidget(labels_[i]);form->addRow(soundcurrent::i18n::text(effectParameters[i].name),row);
            connect(sliders_[i],&QSlider::valueChanged,this,[this,i](int v){state_.values[i]=v/100.;labels_[i]->setText(QString::number(v)+"%");if(onEdited)onEdited();});}
        if(advanced){auto *box=new QGroupBox(SC_TR("Advanced enhancement controls"));auto *f=new QFormLayout(box);
            for(std::size_t i=5;i<EffectParameterCount;++i){const auto &p=effectParameters[i];spins_[i]=new soundcurrent::AcceleratingDoubleSpinBox;spins_[i]->setRange(p.low,p.high);spins_[i]->setDecimals(2);spins_[i]->setSingleStep(p.step);spins_[i]->setValue(p.initial);spins_[i]->setSuffix(p.unit);spins_[i]->setAccessibleName(soundcurrent::i18n::text(p.name));f->addRow(soundcurrent::i18n::text(p.name),spins_[i]);connect(spins_[i],&QDoubleSpinBox::valueChanged,this,[this,i](double v){state_.values[i]=v;if(onEdited)onEdited();});}form->addRow(box);box->setVisible(false);auto *show=new QPushButton(SC_TR("Show advanced controls"));show->setCheckable(true);form->insertRow(6,show);connect(show,&QPushButton::toggled,this,[box,show](bool on){box->setVisible(on);show->setText(on?SC_TR("Hide advanced controls"):SC_TR("Show advanced controls"));});}
        auto *reset=new QPushButton(SC_TR("Reset enhancements"));form->addRow(reset);connect(reset,&QPushButton::clicked,this,[this]{setSettings({});if(onEdited)onEdited();});
        setToolTip(SC_TR("Bass adds low-frequency weight; Clarity adds high-frequency detail; Ambience adds room reflections; Surround widens stereo; Dynamic Boost compresses and raises quieter material with a peak ceiling. Boosting can increase output level."));
    }
    EnhancementSettings settings() const {return state_;}
    void setSettings(const EnhancementSettings &s){if(!s.valid())return;state_=s;for(std::size_t i=0;i<5;++i){QSignalBlocker b(sliders_[i]);sliders_[i]->setValue(int(std::lround(s.values[i]*100)));labels_[i]->setText(QString::number(sliders_[i]->value())+"%");}for(std::size_t i=5;i<EffectParameterCount;++i)if(spins_[i]){QSignalBlocker b(spins_[i]);spins_[i]->setValue(s.values[i]);}}
    std::function<void()> onEdited;
private:
    EnhancementSettings state_;
    std::array<QSlider *,5> sliders_{};
    std::array<QLabel *,5> labels_{};
    std::array<QDoubleSpinBox *,EffectParameterCount> spins_{};
};
}
