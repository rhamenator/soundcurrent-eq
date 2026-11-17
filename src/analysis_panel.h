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
class QVBoxLayout;
class QTimer;
namespace soundcurrent::analysis {
class ScopeCanvas;
class Panel : public QWidget {
public:
    explicit Panel(QWidget *parent=nullptr);
    ~Panel() override;
    std::function<void(const QString &,bool)> onActivity;
    std::function<QVector<QPair<QString,QString>>()> microphoneSources;
    std::function<void(equipment::Profile)> onProfile;
    // endFrame: absolute frame index of the end of `pcm` in the capture's sample counter (-1 if the source has none).
    void pushPcm(const QByteArray &,qint64 endFrame=-1);
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
    bool saveImage(const QString &path);
    void toggleSettings();
    bool settingsVisible() const;
    void refreshSources();
    void captureError(const QString &);
private:
    ScopeCanvas *canvas_=nullptr;
    QComboBox *view_,*source_;
    QCheckBox *freeze_;
    QLabel *readout_,*help_;
    QPushButton *reference_,*profile_;
    QString referenceSource_;
    bool sourceReady_=true,captureFailed_=false,suspended_=false;
    QWidget *settingsPanel_=nullptr;QPushButton *settingsButton_=nullptr;QLabel *feedback_=nullptr;QTimer *commit_=nullptr;
    std::function<Settings()> readControls_;std::function<void(const Settings &)> writeControls_;std::function<void()> controlsEnabled_;
    bool loading_=false;
    void buildSettings(QVBoxLayout *root);
    void availability();
    void suspendForDialog(bool);
};
}
