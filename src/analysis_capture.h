// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "localization_text.h"
#include "microphone_access.h"
#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <functional>
#include <csignal>
#ifdef Q_OS_WIN
#include "windows_audio.h"
#else
#include <sys/prctl.h>
#endif
namespace soundcurrent::analysis {
// Analysis-only microphone tap. Never changes default devices or feeds speakers.
class Capture : public QObject {
public:
    explicit Capture(QObject *parent=nullptr):QObject(parent){
        timer_.setInterval(40);
        connect(&timer_,&QTimer::timeout,this,[this]{
            if(running_ && !playback_ && !soundcurrent::microphone::allowed()){stop();if(onError)onError(SC_TR("Microphone access is disabled."));return;}
#ifdef Q_OS_WIN
            if(!recorder_.running()){stop();if(onError)onError(captureError());return;}
            auto samples=recorder_.take();if(!samples.empty())append(QByteArray(reinterpret_cast<const char *>(samples.data()),int(samples.size()*2)));
#endif
        });
#ifndef Q_OS_WIN
        connect(&process_,&QProcess::readyReadStandardOutput,this,[this]{append(process_.readAllStandardOutput());});
        connect(&process_,&QProcess::errorOccurred,this,[this]{if(running_){stop();if(onError)onError(captureError());}});
        connect(&process_,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this]{if(running_){stop();if(onError)onError(captureError());}});
#endif
    }
    ~Capture() override {onPcm={};onError={};stop();}
    std::function<void(const QByteArray &)> onPcm;
    std::function<void(const QString &)> onError;
    bool running() const {return running_;}
    void start(const QString &source,bool playback=false){
        if(!playback && !soundcurrent::microphone::allowed()){stop();if(onError)onError(SC_TR("Microphone access is disabled."));return;}
        if(running_ && source==source_ && playback==playback_)return;
        stop();playback_=playback;if(source.isEmpty()){if(onError)onError(captureError());return;}source_=source;running_=true;
#ifdef Q_OS_WIN
        try{recorder_.start(source.toStdWString(),48000,2,playback);timer_.start();}catch(...){running_=false;if(onError)onError(captureError());}
#else
        process_.setProgram("parec");process_.setArguments({"--raw","-d",playback?source+".monitor":source,"--format=s16le","--rate=48000","--channels=2","--latency-msec=20"});
        process_.setChildProcessModifier([]{prctl(PR_SET_PDEATHSIG,SIGTERM);});process_.start();timer_.start();
#endif
    }
    void stop(){running_=false;timer_.stop();
#ifdef Q_OS_WIN
        recorder_.stop();
#else
        if(process_.state()!=QProcess::NotRunning){process_.terminate();if(!process_.waitForFinished(300)){process_.kill();process_.waitForFinished(300);}}
#endif
        pcm_.clear();
    }
private:
    QString captureError() const{return playback_?SC_TR("Could not start playback monitoring"):SC_TR("Could not start microphone capture");}
    void append(const QByteArray &bytes){
        if(!running_)return;
        if(!playback_ && !soundcurrent::microphone::allowed()){stop();return;}
        // Keep framing intact even when a pipe read ends between stereo frames.
        pcm_.append(bytes);if(pcm_.size()>131072+3){const auto excess=pcm_.size()-131072;pcm_.remove(0,(excess+3)&~qsizetype(3));}
        const auto aligned=pcm_.size()&~qsizetype(3);if(aligned && onPcm)onPcm(pcm_.left(aligned));
    }
    QProcess process_;QTimer timer_;QByteArray pcm_;QString source_;bool running_=false,playback_=false;
#ifdef Q_OS_WIN
    WindowsRecorder recorder_;
#endif
};
}
