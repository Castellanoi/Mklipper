/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QClipboard>
#include <QString>

class QMimeData;

struct ClipboardSourceInfo {
    QString app;
    QString title;
    QString url;

    bool isEmpty() const { return app.isEmpty() && title.isEmpty() && url.isEmpty(); }
};

ClipboardSourceInfo captureClipboardSource(QClipboard::Mode mode, const QMimeData *mimeData);
QString formatCompactSourceLabel(const QString &app, const QString &title, const QString &url);
QString extractSourceUrlFromMime(const QMimeData *mimeData);
QString prettifyAppName(const QString &rawClass, const QString &rawName);
QString cleanWindowTitle(const QString &rawTitle, const QString &appName);
