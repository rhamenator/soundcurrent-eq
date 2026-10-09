// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QByteArray>
#include <QStringList>
#include "localization_text.h"
namespace soundcurrent::i18n {
// Worker pipes can split UTF-8 sequences and lines at arbitrary byte offsets.
// Decode only complete lines (or the final tail), outside audio callbacks.
class WorkerMessageBuffer {
public:
    static constexpr qsizetype MaxLineBytes=64*1024;
    QStringList append(const QByteArray &bytes, bool final=false) {
        QStringList result;
        auto flush=[&] {
            if(!overflow_) {
                auto message=QString::fromUtf8(pending_);
                if(message.endsWith('\r'))message.chop(1);
                if(!message.trimmed().isEmpty())result.append(message);
            }
            pending_.clear();overflow_=false;
        };
        for(const auto byte:bytes) {
            if(byte=='\n')flush();
            else if(!overflow_) {
                if(pending_.size()==MaxLineBytes){pending_.clear();overflow_=true;}
                else pending_.append(byte);
            }
        }
        if(final)flush();
        return result;
    }
    void reset(){pending_.clear();overflow_=false;}
    qsizetype pendingBytes() const{return pending_.size();}
private:
    QByteArray pending_;
    bool overflow_=false;
 };
// Shared presentation state retains the translated failure and its continuation
// lines independently of whichever progress/detail line is currently visible.
class CalibrationMessageState {
public:
    QString append(const QByteArray &bytes, bool final=false) {
        QString last;
        for(const auto &message:buffer_.append(bytes,final)) {
            last=message;
            if(isCalibrationFailureMessage(message))failure_=message;
            else if(!failure_.isEmpty() && failure_.size()+message.size()+1<=WorkerMessageBuffer::MaxLineBytes)
                failure_+=QStringLiteral("\n")+message;
        }
        return last;
    }
    void reset(){buffer_.reset();failure_.clear();}
    QString failure() const{return failure_;}
private:
    WorkerMessageBuffer buffer_;
    QString failure_;
};
}
