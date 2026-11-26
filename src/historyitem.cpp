/*
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "historyitem.h"

#include <QSqlQuery>
#include <algorithm>

using namespace Qt::StringLiterals;

HistoryItem::HistoryItem(QString uuid,
                         const QStringList &mimeTypes,
                         QString text,
                         QString sourceApp,
                         QString sourceTitle,
                         QString sourceUrl,
                         qreal addedTime,
                         qreal lastUsedTime)
    : m_uuid(std::move(uuid))
    , m_text(std::move(text))
    , m_sourceApp(std::move(sourceApp))
    , m_sourceTitle(std::move(sourceTitle))
    , m_sourceUrl(std::move(sourceUrl))
    , m_addedTime(addedTime)
    , m_lastUsedTime(lastUsedTime > 0.0 ? lastUsedTime : addedTime)
{
    if (std::ranges::any_of(mimeTypes, [](const QString &mimeType) {
            return mimeType.startsWith(u"text/");
        })) {
        m_types |= HistoryItemType::Text;
    }
    if (mimeTypes.contains(u"text/uri-list"_s)) {
        m_types |= HistoryItemType::Url;
    }
    if (std::ranges::any_of(mimeTypes, [](const QString &mimeType) {
            return mimeType.startsWith(u"image/") || mimeType == u"application/x-qt-image";
        })) {
        m_types |= HistoryItemType::Image;
    }
}

HistoryItemType HistoryItem::type() const
{
    if (m_types & HistoryItemType::Image) {
        const QString trimmed = m_text.trimmed();
        if (trimmed.isEmpty()
            || (!trimmed.contains(QLatin1Char(' ')) && !trimmed.contains(QLatin1Char('\n'))
                && (trimmed.startsWith(u"http://"_s, Qt::CaseInsensitive)
                    || trimmed.startsWith(u"https://"_s, Qt::CaseInsensitive)))) {
            return HistoryItemType::Image;
        }
    }
    if (m_types & HistoryItemType::Url) {
        return HistoryItemType::Url;
    } else if ((m_types & HistoryItemType::Text) && !m_text.isEmpty()) {
        return HistoryItemType::Text;
    } else if (m_types & HistoryItemType::Image) {
        return HistoryItemType::Image;
    }
    return HistoryItemType::Text;
}

bool HistoryItem::isValidUuid(const QString &uuid)
{
    if (uuid.size() != 40) {
        return false;
    }
    for (const QChar ch : uuid) {
        const ushort c = ch.unicode();
        const bool isHex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!isHex) {
            return false;
        }
    }
    return true;
}

HistoryItemPtr HistoryItem::create(const QSqlQuery &query)
{
    QString uuid = query.value(u"uuid"_s).toString();
    if (!isValidUuid(uuid)) {
        return {};
    }
    QStringList mimeTypes = query.value(u"mimetypes"_s).toString().split(u',', Qt::SkipEmptyParts);
    if (mimeTypes.isEmpty()) {
        return {};
    }
    QString text = query.value(u"text"_s).toString();
    QString sourceApp = query.value(u"source_app"_s).toString();
    QString sourceTitle = query.value(u"source_title"_s).toString();
    QString sourceUrl = query.value(u"source_url"_s).toString();
    const qreal addedTime = query.value(u"added_time"_s).toDouble();
    const qreal lastUsedTime = query.value(u"last_used_time"_s).toDouble();
    return std::make_shared<HistoryItem>(std::move(uuid),
                                         mimeTypes,
                                         std::move(text),
                                         std::move(sourceApp),
                                         std::move(sourceTitle),
                                         std::move(sourceUrl),
                                         addedTime,
                                         lastUsedTime);
}
