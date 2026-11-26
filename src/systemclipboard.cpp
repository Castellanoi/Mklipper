/*
    SPDX-FileCopyrightText: Andrew Stanley-Jones <asj@cban.com>
    SPDX-FileCopyrightText: 2004 Esben Mose Hansen <kde@mosehansen.dk>
    SPDX-FileCopyrightText: 2008 Dmitry Suzdalev <dimsuz@gmail.com>
    SPDX-FileCopyrightText: 2024 Fushan Wen <qydwhotmail@gmail.com>
    SPDX-FileCopyrightText: 2026 Mklipper contributors
    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "systemclipboard.h"

#include "mimetypes.h"
#include "settings.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QMap>
#include <QMimeData>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>
#include <ranges>

using namespace Qt::StringLiterals;

namespace
{
constexpr int MAX_CLIPBOARD_CHANGES = 10;
}

std::shared_ptr<SystemClipboard> SystemClipboard::self()
{
    static std::weak_ptr<SystemClipboard> s_clip;
    if (s_clip.expired()) {
        std::shared_ptr<SystemClipboard> ptr{new SystemClipboard};
        s_clip = ptr;
        return ptr;
    }
    return s_clip.lock();
}

SystemClipboard::SystemClipboard()
    : QObject(nullptr)
    , m_clip(QGuiApplication::clipboard())
    , m_dataFolder(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + u"/mklipper"_s)
{
    if (m_clip) {
        connect(m_clip, &QClipboard::changed, this, [this](QClipboard::Mode mode) {
            if (mode == QClipboard::Clipboard || mode == QClipboard::Selection) {
                checkClipData(mode);
            }
        });
    }

    m_pendingCheckTimer.setSingleShot(true);
    connect(&m_pendingCheckTimer, &QTimer::timeout, this, &SystemClipboard::slotCheckPending);
    connect(&m_overflowClearTimer, &QTimer::timeout, this, &SystemClipboard::slotClearOverflow);
}

void SystemClipboard::checkClipData(QClipboard::Mode mode, const QMimeData *data)
{
    if ((mode == QClipboard::Clipboard && m_clipboardLockLevel > 0)
        || (mode == QClipboard::Selection && m_selectionLockLevel > 0)) {
        return;
    }

    // Flood protection (e.g. apps that re-claim clipboard repeatedly)
    if (!data) {
        if (!m_overflowClearTimer.isActive()) {
            m_overflowClearTimer.start(1000);
        }
        if (++m_overflowCounter > MAX_CLIPBOARD_CHANGES) {
            if (mode == QClipboard::Selection) {
                m_pendingContentsCheck = true;
                m_pendingCheckTimer.start(250);
            }
            return;
        }
    }

    IgnoreLock lock(mode == QClipboard::Selection ? m_selectionLockLevel : m_clipboardLockLevel);

    if (!data && m_clip) {
        data = m_clip->mimeData(mode);
    }

    if (!data) {
        Q_EMIT receivedEmptyClipboard(mode);
        return;
    }

    if (data->formats().isEmpty()) {
        // Empty clipboard on X11 when source application closes
        Q_EMIT receivedEmptyClipboard(mode);
        return;
    }

    if (!data->hasUrls() && !data->hasText() && !data->hasImage()) {
        return;
    }

    if (data->hasFormat(u"application/x-kde-syncselection"_s)) {
        return;
    }

    Q_EMIT newClipData(mode, data);
}

void SystemClipboard::clear(SelectionMode mode)
{
    if (!m_clip) {
        return;
    }
    if (mode & Selection) {
        IgnoreLock lock(m_selectionLockLevel);
        m_clip->clear(QClipboard::Selection);
    }
    if (mode & Clipboard) {
        IgnoreLock lock(m_clipboardLockLevel);
        m_clip->clear(QClipboard::Clipboard);
    }
}

QMimeData *SystemClipboard::loadMimeDataFromDatabase(const HistoryItemConstPtr &item) const
{
    if (!item) {
        return nullptr;
    }

    QSqlDatabase db = QSqlDatabase::database(u"mklipper"_s);
    if (!db.isOpen()) {
        auto *fallback = new QMimeData;
        fallback->setText(item->text());
        return fallback;
    }

    if (!HistoryItem::isValidUuid(item->uuid())) {
        return nullptr;
    }

    QMap<qint64, std::pair<QString, QByteArray>> mimeDataStore;
    QSqlQuery query(db);
    query.prepare(u"SELECT mimetype, data_uuid, rowid FROM aux WHERE uuid = ? ORDER BY rowid ASC"_s);
    query.addBindValue(item->uuid());
    if (query.exec()) {
        while (query.next()) {
            const QString mimeType = query.value(0).toString();
            const QString dataUuid = query.value(1).toString();
            const qint64 rowId = query.value(2).toLongLong();
            if (mimeType.isEmpty() || !HistoryItem::isValidUuid(dataUuid)) {
                continue;
            }
            const QString dataPath = m_dataFolder + u"/data/"_s + item->uuid() + u'/' + dataUuid;
            QFile file(dataPath);
            if (file.open(QIODevice::ReadOnly)) {
                mimeDataStore.insert(rowId, {mimeType, file.readAll()});
            }
        }
    }

    if (mimeDataStore.isEmpty()) {
        auto *fallback = new QMimeData;
        fallback->setText(item->text());
        return fallback;
    }

    if (MklipperSettings::self()->pastePlainText() && item->type() != HistoryItemType::Image) {
        bool hasUriList = false;
        QString plainText;
        bool hasPlainText = false;
        for (auto it = mimeDataStore.begin(); it != mimeDataStore.end(); ++it) {
            const auto &[format, bytes] = *it;
            if (format == MimeTypes::uriList) {
                hasUriList = true;
                break;
            }
            if ((format == MimeTypes::plainText || format == MimeTypes::plainUtf8Text) && !hasPlainText) {
                plainText = QString::fromUtf8(bytes);
                hasPlainText = true;
            }
        }
        if (!hasUriList) {
            if (!hasPlainText) {
                plainText = item->text();
            }
            if (!plainText.isEmpty()) {
                auto *plainMime = new QMimeData;
                plainMime->setText(plainText);
                return plainMime;
            }
        }
    }

    auto pngIt = std::ranges::find_if(mimeDataStore, [](const auto &pair) {
        return pair.first == MimeTypes::png;
    });

    auto lastImageIt = mimeDataStore.end();
    for (auto it = mimeDataStore.begin(); it != mimeDataStore.end(); ++it) {
        if (it->first.startsWith(MimeTypes::imagePrefix)) {
            lastImageIt = it;
        }
    }

    bool hasPng = (pngIt != mimeDataStore.end());
    auto *mimeData = new QMimeData;
    for (auto it = mimeDataStore.begin(); it != mimeDataStore.end(); ++it) {
        const auto &[format, bytes] = *it;
        if (format == MimeTypes::plainText || format == MimeTypes::plainUtf8Text) {
            if (!mimeData->hasText()) {
                mimeData->setText(QString::fromUtf8(bytes));
            }
        } else {
            mimeData->setData(format, bytes);
        }

        if (it == pngIt || (!hasPng && it == lastImageIt)) {
            const QImage image = QImage::fromData(bytes);
            if (!image.isNull()) {
                mimeData->setImageData(image);
                hasPng = true;
            }
        }
    }

    return mimeData;
}

void SystemClipboard::setMimeData(const HistoryItemConstPtr &item, SelectionMode mode, ClipboardUpdateReason updateReason)
{
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()) || !item) {
        return;
    }
    QMimeData *selData = (mode & Selection) ? loadMimeDataFromDatabase(item) : nullptr;
    QMimeData *clipData = (mode & Clipboard) ? loadMimeDataFromDatabase(item) : nullptr;
    setMimeDataInternal(selData, clipData, updateReason);
}

void SystemClipboard::setMimeData(const QMimeData *data, SelectionMode mode, ClipboardUpdateReason updateReason)
{
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()) || !data) {
        return;
    }

    const QStringList formats = data->formats();
    auto cloneData = [data, &formats]() {
        auto *mimeData = new QMimeData;
        if (data->hasImage()) {
            mimeData->setImageData(data->imageData());
        }
        if (data->hasText()) {
            mimeData->setText(data->text());
        }
        for (const QString &format : formats) {
            if (format.startsWith(MimeTypes::plainText)
                || format.startsWith(MimeTypes::imagePrefix)
                || format == MimeTypes::qtImage) {
                continue;
            }
            mimeData->setData(format, data->data(format));
        }
        return mimeData;
    };

    QMimeData *selData = (mode & Selection) ? cloneData() : nullptr;
    QMimeData *clipData = (mode & Clipboard) ? cloneData() : nullptr;
    setMimeDataInternal(selData, clipData, updateReason);
}

bool SystemClipboard::isLocked(QClipboard::Mode mode) const
{
    return (mode == QClipboard::Selection ? m_selectionLockLevel : m_clipboardLockLevel) > 0;
}

void SystemClipboard::slotClearOverflow()
{
    m_overflowClearTimer.stop();
    m_overflowCounter = 0;
}

void SystemClipboard::slotCheckPending()
{
    if (!m_pendingContentsCheck) {
        return;
    }
    m_pendingContentsCheck = false;
    checkClipData(QClipboard::Selection);
}

void SystemClipboard::setMimeDataInternal(QMimeData *selectionMimeData, QMimeData *clipboardMimeData, ClipboardUpdateReason updateReason)
{
    if (!m_clip) {
        delete selectionMimeData;
        delete clipboardMimeData;
        return;
    }

    if (selectionMimeData) {
        IgnoreLock lock(m_selectionLockLevel);
        if (updateReason == ClipboardUpdateReason::PreventEmptyClipboard) {
            selectionMimeData->setData(u"application/x-kde-onlyReplaceEmpty"_s, "1");
        } else if (updateReason == ClipboardUpdateReason::SyncSelection) {
            selectionMimeData->setData(u"application/x-kde-syncselection"_s, "1");
        }
        m_clip->setMimeData(selectionMimeData, QClipboard::Selection);
    }

    if (clipboardMimeData) {
        if (updateReason == ClipboardUpdateReason::PreventEmptyClipboard) {
            clipboardMimeData->setData(u"application/x-kde-onlyReplaceEmpty"_s, "1");
        } else if (updateReason == ClipboardUpdateReason::SyncSelection) {
            clipboardMimeData->setData(u"application/x-kde-syncselection"_s, "1");
        }
        QMetaObject::invokeMethod(
            this,
            [this, clipboardMimeData]() {
                IgnoreLock lock(m_clipboardLockLevel);
                m_clip->setMimeData(clipboardMimeData, QClipboard::Clipboard);
            },
            updateReason == ClipboardUpdateReason::SyncSelection ? Qt::QueuedConnection : Qt::DirectConnection);
    }
}
