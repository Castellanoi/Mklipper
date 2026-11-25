/*
    SPDX-FileCopyrightText: Andrew Stanley-Jones <asj@cban.com>
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2008 Dmitry Suzdalev <dimsuz@gmail.com>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include <QClipboard>
#include <QObject>
#include <QTimer>

#include "historyitem.h"

class QMimeData;

struct IgnoreLock {
    explicit IgnoreLock(int &lockLevel)
        : m_ref(lockLevel)
    {
        ++m_ref;
    }
    ~IgnoreLock()
    {
        --m_ref;
    }
    IgnoreLock(const IgnoreLock &) = delete;
    IgnoreLock &operator=(const IgnoreLock &) = delete;

private:
    int &m_ref;
};

class SystemClipboard : public QObject
{
    Q_OBJECT

public:
    enum SelectionMode {
        Clipboard = 1 << 1,
        Selection = 1 << 2,
    };

    enum class ClipboardUpdateReason {
        UpdateClipboard,
        SyncSelection,
        PreventEmptyClipboard,
    };

    static std::shared_ptr<SystemClipboard> self();
    ~SystemClipboard() override = default;

    void setDataFolder(const QString &folder) { m_dataFolder = folder; }

    void checkClipData(QClipboard::Mode mode, const QMimeData *data = nullptr);
    void clear(SelectionMode mode = SelectionMode(Selection | Clipboard));
    void setMimeData(const HistoryItemConstPtr &item,
                     SelectionMode mode,
                     ClipboardUpdateReason updateReason = ClipboardUpdateReason::UpdateClipboard);
    void setMimeData(const QMimeData *mimeData,
                     SelectionMode mode,
                     ClipboardUpdateReason updateReason = ClipboardUpdateReason::UpdateClipboard);

    bool isLocked(QClipboard::Mode mode) const;

Q_SIGNALS:
    void newClipData(QClipboard::Mode mode, const QMimeData *data);
    void receivedEmptyClipboard(QClipboard::Mode mode);

private Q_SLOTS:
    void slotClearOverflow();
    void slotCheckPending();

private:
    SystemClipboard();

    QMimeData *loadMimeDataFromDatabase(const HistoryItemConstPtr &item) const;
    void setMimeDataInternal(QMimeData *selectionMimeData, QMimeData *clipboardMimeData, ClipboardUpdateReason updateReason);

    QClipboard *m_clip = nullptr;
    QString m_dataFolder;
    QTimer m_overflowClearTimer;
    QTimer m_pendingCheckTimer;
    bool m_pendingContentsCheck = false;
    int m_overflowCounter = 0;
    int m_clipboardLockLevel = 0;
    int m_selectionLockLevel = 0;
};
