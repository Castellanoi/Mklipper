/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "notifications.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QVariantMap>

namespace Notifications
{
static quint32 s_lastNotificationId = 0;

void show(const QString &title, const QString &body, int timeoutMs)
{
    QDBusInterface iface(QStringLiteral("org.freedesktop.Notifications"),
                         QStringLiteral("/org/freedesktop/Notifications"),
                         QStringLiteral("org.freedesktop.Notifications"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        return;
    }

    QVariantMap hints;
    hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("mklipper"));

    QDBusMessage reply = iface.call(QStringLiteral("Notify"),
                                    QStringLiteral("Mklipper"),
                                    s_lastNotificationId,
                                    QStringLiteral("mklipper"),
                                    title,
                                    body.toHtmlEscaped(),
                                    QStringList(),
                                    hints,
                                    timeoutMs);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) {
        s_lastNotificationId = reply.arguments().first().toUInt();
    }
}
}
