/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <functional>

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QList>
#include <QObject>
#include <QString>

#include "x11keyboard.h"

struct CinnamonShortcutSpec {
    QString id;        // e.g. "mklipper-popup"
    QString label;     // Human-readable label shown in Cinnamon Keyboard settings
    QString command;   // Command to run, e.g. "/usr/bin/mklipper --toggle"
    QKeySequence key;  // Empty = remove
};

class GlobalShortcutManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalShortcutManager(QObject *parent = nullptr);
    ~GlobalShortcutManager() override;

    static bool isCinnamonDesktop();

    // Native Cinnamon shortcut registration via GSettings (avoids Muffin Super_L sync-grab conflict)
    bool syncCinnamonShortcuts(const QList<CinnamonShortcutSpec> &specs);

    // Generic X11 XGrabKey fallback for non-Cinnamon X11 desktops (dynamic modifier & multi-keycode discovery)
    bool registerShortcut(const QString &id, const QKeySequence &sequence, std::function<void()> callback);
    void unregisterShortcut(const QString &id);
    void unregisterAll();

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    struct GrabbedKey {
        QString id;
        QKeySequence sequence;
        QList<unsigned int> keycodes;
        unsigned int modifiers = 0;
        QList<unsigned int> ignoredMasks;
        std::function<void()> callback;
    };

    void ungrab(const GrabbedKey &entry);
    void refreshAllGrabs();

    X11ModifierMap m_modMap;
    QList<GrabbedKey> m_shortcuts;
};
