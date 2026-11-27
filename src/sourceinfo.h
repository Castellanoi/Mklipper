/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QClipboard>
#include <QString>
#include <QStringList>

class QMimeData;

struct ClipboardSourceInfo {
    QString app;
    QString title;
    QString url;

    bool isEmpty() const { return app.isEmpty() && title.isEmpty() && url.isEmpty(); }
};

struct ClipboardSourceInspection {
    bool ignored = false;
    ClipboardSourceInfo info;
};

// Single-pass X11 inspection: checks ignored apps/lockscreens AND optionally captures source metadata
ClipboardSourceInspection inspectClipboardSource(QClipboard::Mode mode,
                                                 const QMimeData *mimeData,
                                                 const QStringList &extraIgnored = {},
                                                 bool captureDetails = true);
ClipboardSourceInfo captureClipboardSource(QClipboard::Mode mode, const QMimeData *mimeData);
bool isIgnoredAppIdentifier(const QString &raw, const QStringList &extraIgnored = {});
bool isIgnoredClipboardSource(QClipboard::Mode mode, const QStringList &extraIgnored = {});
bool isSafeHttpUrl(const QString &candidate);
QString formatCompactSourceLabel(const QString &app, const QString &title, const QString &url);
QString extractSourceUrlFromMime(const QMimeData *mimeData);
QString prettifyAppName(const QString &rawClass, const QString &rawName);
QString cleanWindowTitle(const QString &rawTitle, const QString &appName);
