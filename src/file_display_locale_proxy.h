// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QFileSystemModel>
#include <QIdentityProxyModel>
#include <QLocale>
namespace soundcurrent::i18n {
// Presentation-only adapter. Edit roles, paths, identity, sorting and file operations
// delegate unchanged to Qt. Only file size/date display uses the app format locale.
class FileDisplayLocaleProxy final : public QIdentityProxyModel {
public:
    using QIdentityProxyModel::QIdentityProxyModel;
    QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override {
        if (role == Qt::DisplayRole && index.isValid() && (index.column()==1 || index.column()==3)) {
            const auto *files=qobject_cast<const QFileSystemModel*>(sourceModel());
            if (files) {
                const auto info=files->fileInfo(mapToSource(index));
                if (index.column()==1 && info.isFile()) return QLocale().formattedDataSize(info.size());
                if (index.column()==3 && info.lastModified().isValid()) return QLocale().toString(info.lastModified(),QLocale::ShortFormat);
            }
        }
        return QIdentityProxyModel::data(index,role);
    }
};
}
