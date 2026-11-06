// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "analysis_scope.h"
#include "equipment_profiles.h"
#include <QWidget>
#include <QStringList>
#include <functional>
class QComboBox;
class QPushButton;
class QLabel;
class QCheckBox;
namespace soundcurrent::analysis {
class ScopeCanvas;
class Panel : public QWidget {
public:
    explicit Panel(QWidget *parent=nullptr);
    ~Panel() override;
    std::function<void(const QString &,bool)> onActivity;
    std::function<QVector<QPair<QString,QString>>()> microphoneSources;
    std::function<void(equipment::Profile)> onProfile;
    void pushPcm(const QByteArray &);
    bool active() const;
    QString source() const;
    Engine &engine();
    QSize historyPixelSize() const;
    void setView(View);
    View view() const;
    bool applySettings(const Settings &);
    bool captureReference();
    std::optional<equipment::Profile> microphoneProfile() const;
    void reset();
    void settingsDialog();
    void refreshSources();
    void captureError(const QString &);
private:
    ScopeCanvas *canvas_;
    QComboBox *view_,*source_;
    QCheckBox *freeze_;
    QLabel *readout_,*help_;
    QPushButton *reference_,*profile_;
    QString referenceSource_;
    bool sourceReady_=true,captureFailed_=false,suspended_=false;
    void availability();
    void suspendForDialog(bool);
};
}
