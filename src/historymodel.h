/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>
#include <optional>

#include <QAbstractListModel>
#include <QClipboard>
#include <QHash>
#include <QPointer>
#include <QSqlDatabase>

#include "historyitem.h"
#include "sourceinfo.h"
#include "systemclipboard.h"

class URLGrabber;

class HistoryModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RoleType {
        HistoryItemConstPtrRole = Qt::UserRole,
        UuidRole,
        TypeRole,
        TypeIntRole,
        ImageUrlRole,
        ImageSizeRole,
        StarredRole,
        HasActionRole,
        SourceAppRole,
        SourceTitleRole,
        SourceUrlRole,
        AddedTimeRole,
        LastUsedTimeRole,
    };
    Q_ENUM(RoleType)

    [[nodiscard]] static std::shared_ptr<HistoryModel> self();
    ~HistoryModel() override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    QHash<int, QByteArray> roleNames() const override;

    bool remove(const QString &uuid, bool askConfirmation = true, QWidget *dialogParent = nullptr);
    int starredCount() const { return m_starredCount; }
    bool isItemStarred(const QString &uuid) const;

    qsizetype maxSize() const { return m_maxSize; }
    void setMaxSize(qsizetype size);

    void clear();
    void clearNonStarredHistory();
    void clearHistoryInteractive(QWidget *dialogParent = nullptr);

    void clearHasPassword();
    void clearPasswords();
    void clearBuffers();

    void moveToTop(const QString &uuid, std::optional<bool> plainTextOverride = std::nullopt);
    void moveTopToBack();
    void moveBackToTop();

    bool isCapturePaused() const { return m_capturePaused; }
    void setCapturePaused(bool paused);
    bool toggleCapturePaused();

    bool exportImageToFile(const QString &uuid, const QString &destPath) const;
    bool exportStarredToJson(const QString &filePath, int *exportedCount = nullptr) const;
    bool importStarredFromJson(const QString &filePath, int *importedCount = nullptr);

    int indexOf(const QString &uuid) const;
    int indexOf(const HistoryItem *item) const;
    HistoryItemConstPtr first() const;

    bool insert(const QMimeData *mimeData, qreal timestamp = 0, const ClipboardSourceInfo &source = {});
    bool insert(const QString &text);

    bool loadHistory();
    bool saveClipboardHistory();
    void loadSettings();

    bool clipboardHasPassword() const { return m_clipboardHasPassword; }
    bool selectionHasPassword() const { return m_selectionHasPassword; }

    void setURLGrabber(URLGrabber *grabber);

Q_SIGNALS:
    void changed(bool isTop = false);
    void actionInvoked(const HistoryItemConstPtr &item);
    void selectionForAction(const HistoryItemConstPtr &item);
    void hasPasswordChanged();
    void starredCountChanged(int count);
    void capturePausedChanged(bool paused);

private Q_SLOTS:
    void checkClipData(QClipboard::Mode mode, const QMimeData *data);
    void slotReceivedEmptyClipboard(QClipboard::Mode mode);

private:
    explicit HistoryModel();

    void moveToTop(qsizetype row);
    bool persistMimeData(const QString &uuid,
                         const QString &text,
                         const QMimeData *mimeData,
                         qreal timestamp,
                         const ClipboardSourceInfo &source = {});
    static bool saveToFile(const QString &dbFolder, const QByteArray &data, const QString &itemUuid, const QString &dataUuid);
    void removeItemDataDir(const QString &uuid) const;
    void reclaimDiskSpace(bool fullVacuum = false);
    void cleanupOrphanedStorage();
    void enforcePrivateStoragePermissions() const;
    void setHasPassword(QClipboard::Mode mode, bool hasPassword);

    enum class Holding {
        APassword,
        NoPassword,
    };
    SystemClipboard::SelectionMode buffersHolding(Holding holding) const;

    std::shared_ptr<SystemClipboard> m_clip;
    QList<HistoryItemPtr> m_items;
    QHash<QString, bool> m_starredCache;
    QPointer<URLGrabber> m_urlGrabber;
    int m_starredCount = 0;
    QString m_dbFolder;
    QSqlDatabase m_db;
    qsizetype m_maxSize = 50;
    bool m_bSaveImages = true;
    bool m_bNoNullClipboard = true;
    bool m_bSaveSelection = false;
    bool m_bKeepContents = true;
    bool m_bSynchronize = false;
    bool m_bActionOnSelection = false;
    bool m_bPastePlainText = false;
    bool m_bSaveSourceInfo = true;
    bool m_clipboardHasPassword = false;
    bool m_selectionHasPassword = false;
    bool m_capturePaused = false;
    std::optional<bool> m_nextPlainTextOverride;
};
