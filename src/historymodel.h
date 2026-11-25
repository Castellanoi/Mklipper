/*
    SPDX-FileCopyrightText: 2014 Martin Gräßlin <mgraesslin@kde.org>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include <QAbstractListModel>
#include <QClipboard>
#include <QHash>
#include <QPointer>
#include <QSqlDatabase>

#include "historyitem.h"
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

    void moveToTop(const QString &uuid);
    void moveTopToBack();
    void moveBackToTop();

    int indexOf(const QString &uuid) const;
    int indexOf(const HistoryItem *item) const;
    HistoryItemConstPtr first() const;

    bool insert(const QMimeData *mimeData, qreal timestamp = 0);
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

private Q_SLOTS:
    void checkClipData(QClipboard::Mode mode, const QMimeData *data);
    void slotReceivedEmptyClipboard(QClipboard::Mode mode);

private:
    explicit HistoryModel();

    void moveToTop(qsizetype row);
    bool persistMimeData(const QString &uuid, const QString &text, const QMimeData *mimeData, qreal timestamp);
    static bool saveToFile(const QString &dbFolder, const QByteArray &data, const QString &itemUuid, const QString &dataUuid);
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
    bool m_clipboardHasPassword = false;
    bool m_selectionHasPassword = false;
};
