/*
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <QObject>
#include <QWidget>

class X11Paster : public QObject
{
    Q_OBJECT

public:
    explicit X11Paster(QObject *parent = nullptr);

    // Call right before showing the popup to remember which window had focus
    void captureTargetWindow(WId ownWindowId);

    // Call after setting clipboard & hiding (or keeping open) the popup
    void schedulePaste();

    // Bring our own popup window back to focus (used when persistent + close-on-outside-click)
    void activateOwnWindow(WId ownWindowId) const;

Q_SIGNALS:
    void pasteFinished();

private:
    void attemptPaste(int retriesLeft, quint64 serial);
    void sendPasteKeystroke() const;

    unsigned long m_targetWindow = 0;
    bool m_targetIsTerminal = false;
    quint64 m_pasteSerial = 0;
};
