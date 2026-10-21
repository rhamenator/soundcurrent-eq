// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "file_display_locale_proxy.h"
#include <QFileDialog>
namespace soundcurrent::i18n {
struct FileDialogs {
    static QString choose(QWidget *parent,const QString &title,const QString &directory,
                          const QString &filter,QFileDialog::FileMode mode,QFileDialog::AcceptMode accept) {
        QFileDialog dialog(parent,title,directory,filter);
        dialog.setFileMode(mode);dialog.setAcceptMode(accept);
        dialog.setOption(QFileDialog::DontUseNativeDialog);
        if(mode==QFileDialog::Directory) dialog.setOption(QFileDialog::ShowDirsOnly);
        dialog.setProxyModel(new FileDisplayLocaleProxy(&dialog));
        return dialog.exec()==QDialog::Accepted ? dialog.selectedFiles().value(0) : QString();
    }
    static QString getOpenFileName(QWidget *parent,const QString &title,const QString &directory={},const QString &filter={}) {
        if(QLocale()==QLocale::system()) return QFileDialog::getOpenFileName(parent,title,directory,filter);
        return choose(parent,title,directory,filter,QFileDialog::ExistingFile,QFileDialog::AcceptOpen);
    }
    static QString getSaveFileName(QWidget *parent,const QString &title,const QString &directory={},const QString &filter={}) {
        if(QLocale()==QLocale::system()) return QFileDialog::getSaveFileName(parent,title,directory,filter);
        return choose(parent,title,directory,filter,QFileDialog::AnyFile,QFileDialog::AcceptSave);
    }
    static QString getExistingDirectory(QWidget *parent,const QString &title,const QString &directory={}) {
        if(QLocale()==QLocale::system()) return QFileDialog::getExistingDirectory(parent,title,directory);
        return choose(parent,title,directory,{},QFileDialog::Directory,QFileDialog::AcceptOpen);
    }
};
}
