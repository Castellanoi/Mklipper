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
                         QString sourceUrl)
    : m_uuid(std::move(uuid))
    , m_text(std::move(text))
    , m_sourceApp(std::move(sourceApp))
    , m_sourceTitle(std::move(sourceTitle))
    , m_sourceUrl(std::move(sourceUrl))
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
    if (m_types & HistoryItemType::Url) {
        return HistoryItemType::Url;
    } else if ((m_types & HistoryItemType::Text) && !m_text.isEmpty()) {
        return HistoryItemType::Text;
    } else if (m_types & HistoryItemType::Image) {
        return HistoryItemType::Image;
    }
    return HistoryItemType::Text;
}

HistoryItemPtr HistoryItem::create(const QSqlQuery &query)
{
    QString uuid = query.value(u"uuid"_s).toString();
    if (uuid.isEmpty()) {
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
    return std::make_shared<HistoryItem>(std::move(uuid),
                                         mimeTypes,
                                         std::move(text),
                                         std::move(sourceApp),
                                         std::move(sourceTitle),
                                         std::move(sourceUrl));
}
