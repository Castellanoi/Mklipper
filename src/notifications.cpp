/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "notifications.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QVariantMap>

namespace Notifications
{
static quint32 s_lastNotificationId = 0;

void show(const QString &title, const QString &body, int timeoutMs)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }

    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("mklipper"));

    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
                                                      QStringLiteral("/org/freedesktop/Notifications"),
                                                      QStringLiteral("org.freedesktop.Notifications"),
                                                      QStringLiteral("Notify"));
    msg.setArguments({
        QStringLiteral("Mklipper"),
        s_lastNotificationId,
        QStringLiteral("mklipper"),
        title,
        body.toHtmlEscaped(),
        QStringList(),
        hints,
        timeoutMs,
    });

    const QDBusPendingCall pending = bus.asyncCall(msg, 2000);
    auto *watcher = new QDBusPendingCallWatcher(pending, QCoreApplication::instance());
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher, [](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<quint32> reply = *w;
        if (!reply.isError()) {
            s_lastNotificationId = reply.value();
        }
        w->deleteLater();
    });
}
}
