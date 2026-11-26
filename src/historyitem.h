/*
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#pragma once

#include <memory>

#include <QFlags>
#include <QMetaType>
#include <QString>
#include <QStringList>

class QSqlQuery;

class HistoryItem;
using HistoryItemPtr = std::shared_ptr<HistoryItem>;
using HistoryItemConstPtr = std::shared_ptr<const HistoryItem>;

enum class HistoryItemType {
    Unknown = 1 << 0,
    Text = 1 << 1,
    Image = 1 << 2,
    Url = 1 << 3,
};
Q_DECLARE_FLAGS(HistoryItemTypes, HistoryItemType)
Q_DECLARE_OPERATORS_FOR_FLAGS(HistoryItemTypes)
Q_DECLARE_METATYPE(HistoryItemType)
Q_DECLARE_METATYPE(HistoryItemConstPtr)

class HistoryItem
{
public:
    explicit HistoryItem(QString uuid,
                         const QStringList &mimeTypes,
                         QString text,
                         QString sourceApp = {},
                         QString sourceTitle = {},
                         QString sourceUrl = {});
    ~HistoryItem() = default;

    HistoryItemType type() const;
    HistoryItemTypes allTypes() const { return m_types; }
    QString text() const { return m_text; }
    const QString &uuid() const { return m_uuid; }
    const QString &sourceApp() const { return m_sourceApp; }
    const QString &sourceTitle() const { return m_sourceTitle; }
    const QString &sourceUrl() const { return m_sourceUrl; }
    bool hasSourceInfo() const { return !m_sourceApp.isEmpty() || !m_sourceTitle.isEmpty() || !m_sourceUrl.isEmpty(); }
    void setSourceInfo(QString app, QString title, QString url)
    {
        m_sourceApp = std::move(app);
        m_sourceTitle = std::move(title);
        m_sourceUrl = std::move(url);
    }

    bool operator==(const HistoryItem &rhs) const { return m_uuid == rhs.m_uuid; }

    static HistoryItemPtr create(const QSqlQuery &query);
    static bool isValidUuid(const QString &uuid);

private:
    QString m_uuid;
    HistoryItemTypes m_types = HistoryItemType::Unknown;
    QString m_text;
    QString m_sourceApp;
    QString m_sourceTitle;
    QString m_sourceUrl;
};
