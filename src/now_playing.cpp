// SPDX-License-Identifier: GPL-3.0-only
#include "now_playing.h"
#ifndef Q_OS_WIN
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QVariantMap>
#endif
namespace soundcurrent {
#ifndef Q_OS_WIN
static QString clean(QString text) {
    // Titles come from other programs: drop control characters and bound the length.
    for(QChar &c:text)if(c.unicode()<32 || c==QChar(0x7f))c=' ';
    text=text.simplified();if(text.size()>200)text=text.left(199)+QString::fromUtf8("…");
    return text;
}
QString nowPlayingText() {
    auto bus=QDBusConnection::sessionBus();
    if(!bus.isConnected() || !bus.interface())return {};
    const auto names=bus.interface()->registeredServiceNames();
    if(!names.isValid())return {};
    for(const QString &name:names.value()){
        if(!name.startsWith(QStringLiteral("org.mpris.MediaPlayer2.")))continue;
        QDBusInterface properties(name,QStringLiteral("/org/mpris/MediaPlayer2"),QStringLiteral("org.freedesktop.DBus.Properties"),bus);
        properties.setTimeout(250);
        auto get=[&](const char *property){const QDBusReply<QVariant> reply=properties.call(QStringLiteral("Get"),QStringLiteral("org.mpris.MediaPlayer2.Player"),QString::fromLatin1(property));return reply.isValid()?reply.value():QVariant();};
        if(get("PlaybackStatus").toString()!=QStringLiteral("Playing"))continue;
        const QVariant metadata=get("Metadata");
        QVariantMap map=metadata.canConvert<QDBusArgument>()?qdbus_cast<QVariantMap>(metadata.value<QDBusArgument>()):metadata.toMap();
        const QString title=clean(map.value(QStringLiteral("xesam:title")).toString());
        if(title.isEmpty())continue;
        QVariant artistValue=map.value(QStringLiteral("xesam:artist"));
        QStringList artists=artistValue.canConvert<QDBusArgument>()?qdbus_cast<QStringList>(artistValue.value<QDBusArgument>()):artistValue.toStringList();
        const QString artist=clean(artists.join(QStringLiteral(", ")));
        return artist.isEmpty()?title:artist+QString::fromUtf8(" — ")+title;
    }
    return {};
}
#else
QString nowPlayingText() { return {}; }
#endif
}
